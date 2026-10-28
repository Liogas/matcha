
# EN TETE #
cmake_minimum_required(VERSION 3.21)

project(cpp-pg VERSION 0.1.0 LANGUAGES CXX)

option(CPP_PG_BUILD_TESTS "build cpp-pg tests" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# DEPENDANCES #
# Dans le monorepo, cpp-core est déjà une cible (déclarée avant dans libs/) ;
# seule, la lib la cherche installée.
if(NOT TARGET cpp-core::cpp-core)
	find_package(cpp-core 0.1 CONFIG REQUIRED)
endif()
find_package(PostgreSQL REQUIRED)

# BIBLIOTHEQUE #
add_library(cpp-pg STATIC
	src/Version.cpp
	src/Settings.cpp
	src/SqlState.cpp
	src/Convert.cpp
	src/Params.cpp
	src/Rows.cpp
	src/Connection.cpp
	src/Pool.cpp
)
add_library(cpp-pg::cpp-pg ALIAS cpp-pg)
target_compile_features(cpp-pg PUBLIC cxx_std_23)
target_include_directories(cpp-pg PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(cpp-pg
	PRIVATE CPP_PG_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(cpp-pg
	PUBLIC cpp-core::cpp-core
	PRIVATE PostgreSQL::PostgreSQL
)

# INSTALLATION #
install(TARGETS cpp-pg
	EXPORT cpp-pgTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cpp-pgTargets
	FILE cpp-pgTargets.cmake
	NAMESPACE cpp-pg::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-pg
)

configure_package_config_file(
	cmake/cpp-pgConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/cpp-pgConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-pg
)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/cpp-pgConfigVersion.cmake
	COMPATIBILITY SameMajorVersion
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/cpp-pgConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/cpp-pgConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-pg
)

# TESTS #
if(CPP_PG_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_executable(cpp-pg_tests
        tests/VersionTests.cpp
		tests/SqlStateTests.cpp
		tests/SettingsTests.cpp
		tests/ConvertTests.cpp
		tests/support/PgTest.cpp
		tests/QueryTests.cpp
		tests/TransactionTests.cpp
		tests/PoolTests.cpp
    )
    target_link_libraries(cpp-pg_tests PRIVATE
        cpp-pg::cpp-pg
        GTest::gtest_main
    )
    include(GoogleTest)
    gtest_discover_tests(cpp-pg_tests PROPERTIES
		RESOURCE_LOCK cpp-pg-database)
endif()
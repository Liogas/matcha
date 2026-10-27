
# EN TETE #
cmake_minimum_required(VERSION 3.21)

project(cpp-core VERSION 0.1.0 LANGUAGES CXX)

option(CPP_CORE_BUILD_TESTS "build cpp-core tests" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# BIBLIOTHEQUE #
add_library(cpp-core STATIC
	src/Version.cpp
	src/Error.cpp
	src/Environment.cpp
	src/ConfigReader.cpp
	src/Logger.cpp
)
add_library(cpp-core::cpp-core ALIAS cpp-core)
target_compile_features(cpp-core PUBLIC cxx_std_23)
target_include_directories(cpp-core PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(cpp-core
	PRIVATE CPP_CORE_VERSION="${PROJECT_VERSION}"
)

# INSTALLATION #
install(TARGETS cpp-core
	EXPORT cpp-coreTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cpp-coreTargets
	FILE cpp-coreTargets.cmake
	NAMESPACE cpp-core::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-core
)

configure_package_config_file(
	cmake/cpp-coreConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/cpp-coreConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-core
)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/cpp-coreConfigVersion.cmake
	COMPATIBILITY SameMajorVersion
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/cpp-coreConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/cpp-coreConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-core
)

# TESTS #
if(CPP_CORE_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_executable(cpp-core_tests
        tests/VersionTests.cpp
		tests/ResultTests.cpp
		tests/ConfigReaderTests.cpp
		tests/LoggerTests.cpp
    )
    target_link_libraries(cpp-core_tests PRIVATE
        cpp-core::cpp-core
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(cpp-core_tests)
endif()
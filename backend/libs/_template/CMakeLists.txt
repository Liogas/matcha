
# EN TETE #
cmake_minimum_required(VERSION 3.21)

project(cpp-template VERSION 0.1.0 LANGUAGES CXX)

option(CPP_TEMPLATE_BUILD_TESTS "build cpp-template tests" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# BIBLIOTHEQUE #
add_library(cpp-template STATIC
	src/Version.cpp
)
add_library(cpp-template::cpp-template ALIAS cpp-template)
target_compile_features(cpp-template PUBLIC cxx_std_23)
target_include_directories(cpp-template PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(cpp-template
	PRIVATE CPP_TEMPLATE_VERSION="${PROJECT_VERSION}"
)

# INSTALLATION #
install(TARGETS cpp-template
	EXPORT cpp-templateTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cpp-templateTargets
	FILE cpp-templateTargets.cmake
	NAMESPACE cpp-template::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-template
)

configure_package_config_file(
	cmake/cpp-templateConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/cpp-templateConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-template
)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/cpp-templateConfigVersion.cmake
	COMPATIBILITY SameMajorVersion
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/cpp-templateConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/cpp-templateConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-template
)

# TESTS #
if(CPP_TEMPLATE_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_executable(cpp-template_tests
        tests/VersionTests.cpp
    )
    target_link_libraries(cpp-template_tests PRIVATE
        cpp-template::cpp-template
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(cpp-template_tests)
endif()
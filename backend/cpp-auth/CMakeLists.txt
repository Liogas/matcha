add_library(cpp-auth STATIC
    src/credentials/AuthService.cpp
)

target_include_directories(cpp-auth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(cpp-auth_tests
    tests/credentials/AuthServiceTests.cpp
)

target_link_libraries(cpp-auth_tests PRIVATE
    cpp-auth
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cpp-auth_tests)

add_library(auth STATIC
    src/AuthService.cpp
)

target_include_directories(auth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(auth_tests
    tests/AuthServiceTests.cpp
)

target_link_libraries(auth_tests PRIVATE
    auth
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(auth_tests)

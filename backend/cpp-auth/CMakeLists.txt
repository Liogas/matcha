find_package(nlohmann_json CONFIG REQUIRED)

add_library(cpp-auth STATIC
    src/credentials/AuthService.cpp

    src/jwks/JwksCache.cpp
    src/jwks/JsonJwksParser.cpp
    src/jwks/HttpJwksProvider.cpp
)

target_link_libraries(cpp-auth PRIVATE
    nlohmann_json::nlohmann_json
)

target_include_directories(cpp-auth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(cpp-auth_tests
    tests/credentials/AuthServiceTests.cpp
    
    tests/jwks/JwksCacheTests.cpp
    tests/jwks/JwksParserTests.cpp

    tests/http/HttpClientTests.cpp
)

target_link_libraries(cpp-auth_tests PRIVATE
    cpp-auth
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cpp-auth_tests)

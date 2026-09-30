find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

message(STATUS "HTTPLIB_IS_USING_OPENSSL=${HTTPLIB_IS_USING_OPENSSL}")
message(STATUS "HTTPLIB_IS_COMPILED=${HTTPLIB_IS_COMPILED}")

add_library(cpp-auth STATIC
    src/credentials/AuthService.cpp

    src/jwks/JwksCache.cpp
    src/jwks/JsonJwksParser.cpp
    src/jwks/HttpJwksProvider.cpp

    src/http/CppHttpClient.cpp
    src/http/UrlParser.cpp
)

target_link_libraries(cpp-auth PRIVATE
    nlohmann_json::nlohmann_json
    httplib::httplib
)

target_include_directories(cpp-auth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(cpp-auth_tests
    tests/credentials/AuthServiceTests.cpp
    
    tests/jwks/JwksCacheTests.cpp
    tests/jwks/JwksParserTests.cpp
    tests/jwks/HttpJwksProviderTests.cpp 

    tests/http/HttpClientTests.cpp
    tests/http/UrlParserTests.cpp
)

target_link_libraries(cpp-auth_tests PRIVATE
    cpp-auth
    httplib::httplib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cpp-auth_tests)

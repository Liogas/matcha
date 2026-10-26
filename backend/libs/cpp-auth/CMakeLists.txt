cmake_minimum_required(VERSION 3.21)

set(CPP_AUTH_VERSION 0.1.0)
project(cpp-auth VERSION ${CPP_AUTH_VERSION} LANGUAGES CXX)
option(CPP_AUTH_BUILD_TESTS "Build cpp-auth test" ${PROJECT_IS_TOP_LEVEL})

find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(cpp-auth STATIC
    src/credentials/AuthService.cpp

    src/session/InMemorySessionStore.cpp
    src/session/SessionManager.cpp

    src/jwks/JwksCache.cpp
    src/jwks/JsonJwksParser.cpp
    src/jwks/HttpJwksProvider.cpp

    src/http/CppHttpClient.cpp
    src/http/UrlParser.cpp

    src/time/SystemClock.cpp
    src/time/SystemWallClock.cpp

    src/token/TokenValidator.cpp
    src/token/TokenGenerator.cpp
    src/token/RsaTokenSigner.cpp
    src/token/RsaTokenSignatureVerifier.cpp

    src/crypto/RsaPublicKeyBuilder.cpp

    src/auth/Authenticator.cpp
)

add_library(cpp-auth::cpp-auth ALIAS cpp-auth)

target_compile_features(cpp-auth PUBLIC cxx_std_17)

target_link_libraries(cpp-auth
    PRIVATE
    nlohmann_json::nlohmann_json
    httplib::httplib
    PUBLIC
    Threads::Threads
    OpenSSL::Crypto
    jwt-cpp::jwt-cpp
)

target_include_directories(cpp-auth
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS cpp-auth
    EXPORT cpp-authTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT cpp-authTargets
    FILE cpp-authTargets.cmake
    NAMESPACE cpp-auth::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-auth
)

configure_package_config_file(
    cmake/cpp-authConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cpp-authConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-auth
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cpp-authConfigVersion.cmake
    VERSION ${CPP_AUTH_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cpp-authConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cpp-authConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-auth
)

if(CPP_AUTH_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_executable(cpp-auth_tests
        tests/credentials/AuthServiceTests.cpp

        tests/session/SessionManagerTests.cpp

        tests/jwks/JwksCacheTests.cpp
        tests/jwks/JwksParserTests.cpp
        tests/jwks/HttpJwksProviderTests.cpp

        tests/http/HttpClientTests.cpp
        tests/http/UrlParserTests.cpp

        tests/token/TokenValidatorTests.cpp
        tests/token/TokenValidatorInterfaceTests.cpp
        tests/token/TokenGeneratorTests.cpp
        tests/token/RsaTokenSignerTests.cpp
        tests/token/RsaTokenSignatureVerifierTests.cpp

        tests/crypto/RsaPublicKeyBuilderTests.cpp

        tests/auth/AuthenticatorTests.cpp
    )

    target_compile_definitions(cpp-auth_tests PRIVATE
        CPP_AUTH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(cpp-auth_tests PRIVATE
        cpp-auth
        httplib::httplib
        GTest::gtest_main
        OpenSSL::Crypto
    )

    include(GoogleTest)
    gtest_discover_tests(cpp-auth_tests)
endif()

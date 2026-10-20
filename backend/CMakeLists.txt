cmake_minimum_required(VERSION 3.20)

project(matcha_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(_VCPKG_INSTALLED_DIR "${VCPKG_INSTALLED_DIR}")
set(VCPKG_TARGET_TRIPLET "x64-linux")

include (CTest)
enable_testing()

find_package(Crow CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)

add_subdirectory(cpp-auth)

add_executable(matcha_backend
    src/main.cpp

    src/database/Database.cpp
    src/database/QueryResult.cpp
    src/database/DatabaseInitializer.cpp

    src/repositories/AccountRepository.cpp

    src/security/PasswordHasher.cpp
    src/security/OpaqueToken.cpp
    src/security/EmailVerificationToken.cpp
    src/services/SmtpEmailSender.cpp
    src/security/RedisLoginAttemptLimiter.cpp

    src/http/AuthRoutes.cpp
    src/http/AuthRouteSupport.cpp
    src/http/AuthEmailRoutes.cpp
    src/http/AuthPasswordResetRoutes.cpp
    src/http/AuthSessionRoutes.cpp
)

target_link_libraries(matcha_backend
    PRIVATE cpp-auth::cpp-auth
    PRIVATE Crow::Crow
    PRIVATE PostgreSQL::PostgreSQL
    PRIVATE OpenSSL::Crypto
    PRIVATE CURL::libcurl
    PRIVATE unofficial-sodium::sodium
    PRIVATE redis++::redis++_static
)

add_executable(login_rate_limiter_tests
    tests/security/RedisLoginAttemptLimiterTests.cpp
    src/security/RedisLoginAttemptLimiter.cpp
)

target_include_directories(login_rate_limiter_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(login_rate_limiter_tests
    PRIVATE redis++::redis++_static GTest::gtest_main OpenSSL::Crypto
)

add_executable(user_repository_integration_tests
    tests/integration/UserRepositoryIntegrationTests.cpp
    src/database/Database.cpp
    src/database/QueryResult.cpp
    src/repositories/UserRepository.cpp
)

target_include_directories(user_repository_integration_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(user_repository_integration_tests
    PRIVATE
        cpp-auth::cpp-auth
        PostgreSQL::PostgreSQL
        GTest::gtest_main
)

add_executable(account_repository_integration_tests
    tests/integration/AccountRepositoryIntegrationTests.cpp
    src/database/Database.cpp
    src/database/QueryResult.cpp
    src/repositories/AccountRepository.cpp
)

target_include_directories(account_repository_integration_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(account_repository_integration_tests
    PRIVATE
        cpp-auth::cpp-auth
        PostgreSQL::PostgreSQL
        GTest::gtest_main
)

add_executable(password_hasher_tests
    tests/security/PasswordHasherTests.cpp
    src/security/PasswordHasher.cpp
)

add_executable(email_verification_token_tests
    tests/security/EmailVerificationTokenTests.cpp
    src/security/OpaqueToken.cpp
    src/security/EmailVerificationToken.cpp
)

target_include_directories(email_verification_token_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(email_verification_token_tests
    PRIVATE unofficial-sodium::sodium GTest::gtest_main
)

target_include_directories(password_hasher_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(password_hasher_tests
    PRIVATE
        cpp-auth::cpp-auth
        unofficial-sodium::sodium
        GTest::gtest_main
)

add_executable(auth_integration_tests
    tests/integration/AuthIntegrationTests.cpp
    tests/integration/AuthAuthentificationIntegrationTests.cpp

    src/database/Database.cpp
    src/database/QueryResult.cpp
    
    src/repositories/UserRepository.cpp
    
    src/security/PasswordHasher.cpp
)

target_include_directories(auth_integration_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(auth_integration_tests
    PRIVATE
        cpp-auth::cpp-auth
        PostgreSQL::PostgreSQL
        unofficial-sodium::sodium
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(auth_integration_tests)
gtest_discover_tests(password_hasher_tests)
gtest_discover_tests(user_repository_integration_tests)
gtest_discover_tests(account_repository_integration_tests)
gtest_discover_tests(login_rate_limiter_tests)
gtest_discover_tests(email_verification_token_tests)
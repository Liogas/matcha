cmake_minimum_required(VERSION 3.20)

project(matcha_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(cmake/Warnings.cmake)
set(_VCPKG_INSTALLED_DIR "${VCPKG_INSTALLED_DIR}")
set(VCPKG_TARGET_TRIPLET "x64-linux")

include (CTest)
enable_testing()

find_package(Crow CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips-cpp)

add_subdirectory(libs)

add_executable(matcha_backend
    src/main.cpp

    src/repositories/AccountRepository.cpp
    src/repositories/ProfileRepository.cpp
    src/repositories/LikeRepository.cpp

    src/security/PasswordHasher.cpp
    src/security/OpaqueToken.cpp
    src/security/EmailVerificationToken.cpp
    
    src/services/SmtpEmailSender.cpp
    src/services/ProfilePhotoService.cpp
    src/services/MatchService.cpp
    src/services/ProfilePhotoService.cpp
    
    src/security/RedisLoginAttemptLimiter.cpp
    
    src/images/ImageProcessor.cpp
    src/images/LocalImageStorage.cpp
    
    src/messaging/MessagingRepository.cpp
    src/messaging/MessagingWebSocket.cpp
    src/messaging/MessagingRoutes.cpp

    src/http/AuthRoutes.cpp
    src/http/AuthRouteSupport.cpp
    src/http/AuthEmailRoutes.cpp
    src/http/AuthPasswordResetRoutes.cpp
    src/http/AuthSessionRoutes.cpp
    src/http/ProfileRoutes.cpp
    src/http/LikeRoutes.cpp
)

target_link_libraries(matcha_backend
    PRIVATE cpp-core::cpp-core
    PRIVATE cpp-pg::cpp-pg
    PRIVATE cpp-auth::cpp-auth
    PRIVATE Crow::Crow
    PRIVATE OpenSSL::Crypto
    PRIVATE CURL::libcurl
    PRIVATE unofficial-sodium::sodium
    PRIVATE redis++::redis++_static
    PRIVATE PkgConfig::VIPS
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

# Tests des repositories et de MatchService, sur la base postgres-test
# (variables PG_TEST_*), avec le schéma appliqué depuis migrations/.
add_executable(repository_tests
    tests/support/DatabaseTest.cpp
    tests/integration/AccountRepositoryIntegrationTests.cpp
    tests/integration/ProfileRepositoryTests.cpp
    tests/integration/MatchServiceTests.cpp
    src/repositories/AccountRepository.cpp
    src/repositories/ProfileRepository.cpp
    src/repositories/LikeRepository.cpp
    src/messaging/MessagingRepository.cpp
    src/services/MatchService.cpp
)

target_include_directories(repository_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_compile_definitions(repository_tests
    PRIVATE MATCHA_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/migrations"
)

target_link_libraries(repository_tests
    PRIVATE
        cpp-pg::cpp-pg
        cpp-auth::cpp-auth
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

add_executable(local_image_storage_tests
    tests/images/LocalImageStorageTests.cpp
    src/images/LocalImageStorage.cpp
)

target_include_directories(local_image_storage_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(local_image_storage_tests
    PRIVATE GTest::gtest_main
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

include(GoogleTest)
gtest_discover_tests(password_hasher_tests)
# Tous ces tests partagent la base de test : un seul à la fois, même avec ctest -j.
gtest_discover_tests(repository_tests PROPERTIES RESOURCE_LOCK matcha-test-database)
gtest_discover_tests(login_rate_limiter_tests)
gtest_discover_tests(email_verification_token_tests)
gtest_discover_tests(local_image_storage_tests)
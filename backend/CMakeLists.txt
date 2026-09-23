cmake_minimum_required(VERSION 3.20)

project(matcha_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include (CTest)
enable_testing()

find_package(Crow CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

add_subdirectory(auth)

add_executable(matcha_backend
    src/main.cpp

    src/database/Database.cpp
    src/database/QueryResult.cpp
    src/database/DatabaseInitializer.cpp

    src/repositories/UserRepository.cpp

    src/security/PasswordHasher.cpp

    src/http/AuthRoutes.cpp
)

target_link_libraries(matcha_backend
    PRIVATE auth
    PRIVATE Crow::Crow
    PRIVATE PostgreSQL::PostgreSQL
    PRIVATE unofficial-sodium::sodium
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
        auth
        PostgreSQL::PostgreSQL
        GTest::gtest_main
)

add_executable(password_hasher_tests
    tests/security/PasswordHasherTests.cpp
    src/security/PasswordHasher.cpp
)

target_include_directories(password_hasher_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(password_hasher_tests
    PRIVATE
        auth
        unofficial-sodium::sodium
        GTest::gtest_main
)

add_executable(auth_integration_tests
    tests/integration/AuthIntegrationTests.cpp
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
        auth
        PostgreSQL::PostgreSQL
        unofficial-sodium::sodium
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(auth_integration_tests)
gtest_discover_tests(password_hasher_tests)
gtest_discover_tests(user_repository_integration_tests)
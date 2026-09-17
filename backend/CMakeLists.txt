cmake_minimum_required(VERSION 3.20)

project(matcha_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include (CTest)
enable_testing()

find_package(Crow CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(GTest CONFIG REQUIRED)

add_subdirectory(auth)

add_executable(matcha_backend
    src/main.cpp
    src/database/Database.cpp
    src/database/QueryResult.cpp
    src/database/DatabaseInitializer.cpp
    src/repositories/UserRepository.cpp
)

target_link_libraries(matcha_backend
    PRIVATE Crow::Crow
    PRIVATE PostgreSQL::PostgreSQL
)
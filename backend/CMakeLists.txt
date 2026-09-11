cmake_minimum_required(VERSION 3.20)

project(matcha_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Crow CONFIG REQUIRED)

add_executable(matcha_backend
    src/main.cpp
)

target_link_libraries(matcha_backend
    PRIVATE Crow::Crow
)
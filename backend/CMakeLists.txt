cmake_minimum_required(VERSION 3.20)

project(matcha_backend)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    matcha_backend
    src/main.cpp
)

target_include_directories(
    matcha_backend
    PRIVATE
    /usr/local/include
)
cmake_minimum_required(VERSION 3.21)
project(cpp-core_package_consumer LANGUAGES CXX)

find_package(cpp-core 0.1 CONFIG REQUIRED)

add_executable(package_consumer main.cpp)
target_link_libraries(package_consumer PRIVATE cpp-core::cpp-core)
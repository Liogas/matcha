cmake_minimum_required(VERSION 3.21)
project(cpp-pg_package_consumer LANGUAGES CXX)

find_package(cpp-pg 0.2 CONFIG REQUIRED)

add_executable(package_consumer main.cpp)
target_link_libraries(package_consumer PRIVATE cpp-pg::cpp-pg)
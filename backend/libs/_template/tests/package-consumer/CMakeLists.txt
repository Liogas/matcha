cmake_minimum_required(VERSION 3.21)
project(cpp-template_package_consumer LANGUAGES CXX)

find_package(cpp-template 1.0 CONFIG REQUIRED)

add_executable(package_consumer main.cpp)
target_link_libraries(package_consumer PRIVATE cpp-template::cpp-template)
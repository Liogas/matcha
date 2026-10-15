cmake_minimum_required(VERSION 3.20)
project(cpp_auth_package_consumer LANGUAGES CXX)

find_package(cpp-auth CONFIG REQUIRED)

add_executable(cpp_auth_package_consumer main.cpp)
target_link_libraries(cpp_auth_package_consumer PRIVATE cpp-auth::cpp-auth)

# CMake generated Testfile for 
# Source directory: /app
# Build directory: /app/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/app/build/auth_integration_tests[1]_include.cmake")
include("/app/build/password_hasher_tests[1]_include.cmake")
include("/app/build/user_repository_integration_tests[1]_include.cmake")
subdirs("cpp-auth")

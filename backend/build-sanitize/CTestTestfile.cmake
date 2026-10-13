# CMake generated Testfile for 
# Source directory: /app
# Build directory: /app/build-sanitize
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/app/build-sanitize/auth_integration_tests[1]_include.cmake")
include("/app/build-sanitize/password_hasher_tests[1]_include.cmake")
include("/app/build-sanitize/user_repository_integration_tests[1]_include.cmake")
subdirs("cpp-auth")

grep CPP_AUTH_BUILD_TESTS build/CMakeCache.txt 
rm build/CMakeCache.txt 
exit
cmake --fresh
ls
rm -rf CMakeCache.txt 
exit
pwd
clear
T="-DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR=$VCPKG_INSTALLED_DIR -DVCPKG_MANIFEST_INSTALL=OFF"
cmake -S libs/cpp-auth -B /tmp/alone $T
grep CPP_AUTH_BUILD_TESTS
cmake -S libs/cpp-auth -B /tmp/alone $T
grep CPP_AUTH_BUILD_TESTS /tmp/alone/CMakeCache.txt
cmake --build /tmp/alone -j"$(nproc)"
ctest --test-dir /tmp/alone
clear
cmake --install /tmp/alone --prefix /tmp/inst
ls /tmp/inst/lib/cmake/cpp-auth
cmake -S libs/cpp-auth/tests/package-consumer -B /tmp/cons $T -DCMAKE_PREFIX_PATH=/tmp/inst
cmake --build /tmp/cons
/tmp/cons/cpp_auth_package_consumer && echo OK
exit

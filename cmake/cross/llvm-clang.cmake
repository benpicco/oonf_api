# to use this file, create a build-win32 directory,
# change into the directory and run cmake there:
#
# > mkdir build-clang
# > cd build-clang
# > cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/cross/llvm-clang.cmake ..
# > make

# which compilers to use for C and C++
SET(CMAKE_C_COMPILER clang)
SET(CMAKE_CXX_COMPILER clang++)
SET(CMAKE_LINKER gold)

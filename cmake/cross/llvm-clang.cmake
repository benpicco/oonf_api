# to use this file, create a build-win32 directory,
# change into the directory and run cmake there:
#
# > mkdir build-clang
# > cd build-clang
# > cmake -DCMAKE_TOOLCHAIN_FILE=../files/llvm-clang.cmake ..
# > make

# which compilers to use for C and C++
SET(CMAKE_C_COMPILER clang)
SET(CMAKE_CXX_COMPILER clang++)
SET(CMAKE_LINKER gold)

# here is the target environment located
# SET(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32/)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
#set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
#set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
#set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

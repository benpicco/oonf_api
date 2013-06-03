# to use this file, create a build-win32 directory,
# change into the directory and run cmake there:
#
# > mkdir build-win32
# > cd build-win32
# > cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/cross/win32_on_linux.cmake ..
# > make

# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Windows)

# which compilers to use for C and C++
SET(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
SET(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
SET(CMAKE_RC_COMPILER i686-w64-mingw32-windres)

# here is the target environment located (adapt to your local path)
# SET(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32/)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

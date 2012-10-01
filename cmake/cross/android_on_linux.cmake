# to use this file, create a build-android directory,
# change into the directory and run cmake there:
#
# > mkdir build-android
# > cd build-android
# > cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/cross/android_on_linux.cmake ..
# > make


SET(CMAKE_SYSTEM_NAME Linux)  # Tell CMake we're cross-compiling
include(CMakeForceCompiler)

# Prefix detection only works with compiler id "GNU"
# CMake will look for prefixed g++, cpp, ld, etc. automatically
CMAKE_FORCE_C_COMPILER(arm-linux-androideabi-gcc GNU)

SET(ANDROID TRUE)

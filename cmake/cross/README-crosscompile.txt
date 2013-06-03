Crosscompilation with CMake is not really difficult.

1) create your own build directory for the new build target and change into it

mkdir build-android
cd build-android

2) then run cmake with the right crosscompile template (see /cross for examples)

cmake -D CMAKE_TOOLCHAIN_FILE=../cmake/cross/android_on_linux.cmake ..

3) start building

make

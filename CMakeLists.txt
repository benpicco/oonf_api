project (OONF C)
cmake_minimum_required(VERSION 2.8)

###########################
#### API configuration ####
###########################

set(OONF_MAJOR_VERSION 0)
set(OONF_MINOR_VERSION 7)
set(OONF_PATCH_VERSION 0)

# set library version
set (OONF_VERSION ${OONF_MAJOR_VERSION}.${OONF_MINOR_VERSION}.${OONF_PATCH_VERSION})

# enable cached variables
include (./lib_config.cmake)

# include flags
include (./cmake/cc_flags.cmake)

SET(OONF_SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

######################################
#### Install target configuration ####
######################################

set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR CMake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib/CMake)
endif()
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Make relative paths absolute (needed later on)
foreach(p LIB BIN INCLUDE CMAKE)
  set(var INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()

########################
#### Doxygen target ####
########################

find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files/framework.doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/framework.doxyfile @ONLY)
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/framework.doxyfile
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
endif(DOXYGEN_FOUND)

#########################
#### Compile targets ####
#########################

# remove export set file
FILE(REMOVE "${PROJECT_BINARY_DIR}/OONFLibraryDepends.cmake")

# include build helper
include (cmake/OONFBuildLibrary.cmake)

# include files
include_directories(${PROJECT_BINARY_DIR})
include_directories(external)
include_directories(src-api)

ENABLE_TESTING()

# start compiling core and plugins
add_subdirectory(src-api)
add_subdirectory(src-plugins)
add_subdirectory(external)
add_subdirectory(tests)

###############################
#### Installation handling ####
###############################

# get list of include directories
get_property(INCLUDE_LIST DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE OONF)

# Create a OONFBuildTreeSettings.cmake file for the use from the build tree
configure_file(./cmake/files/OONFBuildTreeSettings.cmake.in
    "${PROJECT_BINARY_DIR}/OONFBuildTreeSettings.cmake" @ONLY)

# Create the OONFConfig.cmake and OONFConfigVersion files
file(RELATIVE_PATH CONF_REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}"
   "${INSTALL_INCLUDE_DIR}")
configure_file(./cmake/files/OONFConfig.cmake.in
    "${PROJECT_BINARY_DIR}/OONFConfig.cmake" @ONLY)
configure_file(./cmake/files/OONFConfigVersion.cmake.in
    "${PROJECT_BINARY_DIR}/OONFConfigVersion.cmake" @ONLY)

# Copy OONFBuildLibrary to build directory
file(COPY ./cmake/OONFBuildLibrary.cmake
     DESTINATION ${PROJECT_BINARY_DIR})

# Install the export set for use with the install-tree
install(EXPORT OONFLibraryDepends DESTINATION
    "${INSTALL_CMAKE_DIR}" COMPONENT dev)

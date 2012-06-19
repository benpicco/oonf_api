project (OONF C)
cmake_minimum_required(VERSION 2.8)

###########################
#### API configuration ####
###########################

# set library version
set (OONF_MAJOR_VERSION 0)
set (OONF_MINOR_VERSION 7)
set (OONF_PATCH_VERSION 0)

# calculate complete library version
set (OONF_VERSION ${OONF_MAJOR_VERSION}.${OONF_MINOR_VERSION}.${OONF_PATCH_VERSION})

# set cached variables
include (./cmake/lib_config.cmake)

# include compiler flags
include (./cmake/cc_flags.cmake)

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

# include build helper
include (cmake/OONFBuildLibrary.cmake)

# include files
include_directories(${PROJECT_BINARY_DIR})
include_directories(external)
include_directories(src-api)

if (NOT OONF_NO_TESTING)
    ENABLE_TESTING()
endif (NOT OONF_NO_TESTING)

# start compiling core and plugins
add_subdirectory(src-api)
add_subdirectory(src-plugins)
add_subdirectory(external)

if (NOT OONF_NO_TESTING)
    add_subdirectory(tests)
endif (NOT OONF_NO_TESTING)

###############################
#### Installation handling ####
###############################

# get list of include directories
get_property (INCLUDE_LIST DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
if (NOT OONF_DO_NOT_REGISTER_PACKAGE)
    export(PACKAGE OONF)
endif (NOT OONF_DO_NOT_REGISTER_PACKAGE)

# Create a OONFBuildTreeSettings.cmake file for the use from the build tree
configure_file (./cmake/files/OONFBuildTreeSettings.cmake.in
    "${PROJECT_BINARY_DIR}/OONFBuildTreeSettings.cmake" @ONLY)

# Create the OONFConfig.cmake and OONFConfigVersion files
file (RELATIVE_PATH CONF_REL_INCLUDE_DIR "/${INSTALL_CMAKE_DIR}"
   "/${INSTALL_INCLUDE_DIR}")
file (RELATIVE_PATH CONF_REL_LIB_DIR "/${INSTALL_CMAKE_DIR}"
   "/${INSTALL_LIB_DIR}")
configure_file (./cmake/files/OONFConfig.cmake.in
    "${PROJECT_BINARY_DIR}/OONFConfig.cmake" @ONLY)
configure_file (./cmake/files/OONFConfigVersion.cmake.in
    "${PROJECT_BINARY_DIR}/OONFConfigVersion.cmake" @ONLY)

# Copy OONFBuildLibrary to build directory
file (COPY ./cmake/OONFBuildLibrary.cmake
     DESTINATION ${PROJECT_BINARY_DIR})
file (COPY ./cmake/OONFLibraryDepends.cmake
     DESTINATION ${PROJECT_BINARY_DIR})

# install cmake files
install (FILES ${PROJECT_BINARY_DIR}/OONFBuildLibrary.cmake
               ${PROJECT_BINARY_DIR}/OONFConfig.cmake
               ${PROJECT_BINARY_DIR}/OONFConfigVersion.cmake
         DESTINATION ${INSTALL_CMAKE_DIR} COMPONENT dev)

# Install the export set for use with the install-tree
install (EXPORT OONFLibraryDepends DESTINATION
    "${INSTALL_CMAKE_DIR}" COMPONENT dev)

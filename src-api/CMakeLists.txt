# build framework
add_subdirectory(common)
IF (WIN32)
    add_subdirectory(regex)
ENDIF(WIN32)
add_subdirectory(config)
add_subdirectory(rfc5444)
add_subdirectory(core)

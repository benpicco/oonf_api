# make sure that oonf_libdata is rebuild everytimes 
SET(GEN_DATA_C ${PROJECT_BINARY_DIR}/oonf_libdata.c)

ADD_CUSTOM_TARGET(LibCleanData ALL
    COMMAND ${CMAKE_COMMAND} -E remove ${GEN_DATA_C}
    COMMENT "Remove old library builddata"
)

ADD_CUSTOM_COMMAND (
    OUTPUT ${GEN_DATA_C}
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/oonf_libdata.c.in
        -DDST=${GEN_DATA_C}
        -DGIT=${PROJECT_SOURCE_DIR}/.git
        -DOONF_LIB_VERSION=${OONF_LIB_VERSION}
        -DCMAKE_SYSTEM=${CMAKE_SYSTEM}
        -DCMAKE_SHARED_LIBRARY_PREFIX=${CMAKE_SHARED_LIBRARY_PREFIX}
        -DCMAKE_SHARED_LIBRARY_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}
        -P ${CMAKE_SOURCE_DIR}/cmake/generate_builddata.cmake
    DEPENDS LibCleanData
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Create new library builddata"
)

SET(OONF_CORE_SRCS oonf_cfg.c
                   oonf_logging.c
                   oonf_logging_cfg.c
                   oonf_plugins.c
                   oonf_subsystem.c
                   ${GEN_DATA_C})

SET(OONF_CORE_INCLUDES oonf_cfg.h
                       oonf_logging.h
                       oonf_logging_cfg.h
                       oonf_plugins.h
                       oonf_subsystem.h
                       oonf_libdata.h
                       os_clock.h
                       )

# add os_specific source(include files
# TODO: add BSD and WIN32
IF(LINUX)
    SET(OONF_CORE_SRCS ${OONF_CORE_SRCS}
                       os_linux/os_clock_linux.c
                       os_generic/os_syslog_generic.c
                       )
    SET(OONF_CORE_INCLUDES ${OONF_CORE_INCLUDES}
                       os_linux/os_clock_linux.h
                       )
ENDIF(LINUX)

oonf_create_library("core" "${OONF_CORE_SRCS}" "${OONF_CORE_INCLUDES}" "rt")

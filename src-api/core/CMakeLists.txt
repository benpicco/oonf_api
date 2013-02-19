# make sure that olsr_libdata is rebuild everytimes 
SET(GEN_DATA_C ${PROJECT_BINARY_DIR}/olsr_libdata.c)

ADD_CUSTOM_TARGET(LibCleanData ALL
    COMMAND ${CMAKE_COMMAND} -E remove ${GEN_DATA_C}
    COMMENT "Remove old library builddata"
)

ADD_CUSTOM_COMMAND (
    OUTPUT ${GEN_DATA_C}
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/olsr_libdata.c.in
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

SET(OONF_CORE_SRCS olsr_callbacks.c
                   olsr_clock.c
                   olsr_interface.c
                   olsr_logging.c
                   olsr_object.c
                   olsr_netaddr_acl.c
                   olsr_packet_socket.c
                   olsr_plugins.c
                   olsr_socket.c
                   olsr_stream_socket.c
                   olsr_timer.c
                   ${GEN_DATA_C})

SET(OONF_CORE_INCLUDES olsr_callbacks.h
                       olsr_clock.h
                       olsr_interface.h
                       olsr_logging.h
                       olsr_memcookie.h
                       olsr_netaddr_acl.h
                       olsr_packet_socket.h
                       olsr_plugins.h
                       olsr_socket.h
                       olsr_subsystem.h
                       olsr_stream_socket.h
                       olsr_timer.h
                       olsr_libdata.h
                       os_clock.h
                       os_net.h
                       os_routing.h
                       os_syslog.h
                       os_system.h
                       )

# add os_specific source(include files
# TODO: add BSD and WIN32
IF(LINUX)
    SET(OONF_CORE_SRCS ${OONF_CORE_SRCS}
                       os_generic/os_net_generic_configsocket.c
                       os_generic/os_net_generic_getsocket.c
                       os_generic/os_net_generic_join_mcast.c
                       os_generic/os_net_generic_sendto.c
                       os_generic/os_net_generic_set_nonblocking.c
                       os_generic/os_syslog_generic.c
                       os_linux/os_net_linux.c
                       os_linux/os_system_linux.c
                       os_linux/os_routing_linux.c
                       os_linux/os_clock_linux.c)
    SET(OONF_CORE_INCLUDES ${OONF_CORE_INCLUDES}
                           os_linux/os_net_linux.h
                           os_linux/os_system_linux.h
                           os_linux/os_routing_linux.h
                           os_linux/os_clock_linux.h)
ENDIF(LINUX)

oonf_create_library("core" "${OONF_CORE_SRCS}" "${OONF_CORE_INCLUDES}" "rt")

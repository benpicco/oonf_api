SET(OONF_SUBSYSTEMS_SRCS oonf_class.c
                         oonf_clock.c
                         oonf_duplicate_set.c
                         oonf_http.c
                         oonf_interface.c
                         oonf_layer2.c
                         oonf_packet_socket.c
                         oonf_rfc5444.c
                         oonf_socket.c
                         oonf_stream_socket.c
                         oonf_telnet.c
                         oonf_timer.c
                         )

SET(OONF_SUBSYSTEMS_INCLUDES oonf_class.h
                             oonf_clock.h
                             oonf_duplicate_set.h
                             oonf_http.h
                             oonf_interface.h
                             oonf_layer2.h
                             oonf_packet_socket.h
                             oonf_rfc5444.h
                             oonf_socket.h
                             oonf_stream_socket.h
                             oonf_telnet.h
                             oonf_timer.h
                             os_net.h
                             os_routing.h
                             os_system.h
                             )

# add os_specific source(include files
# TODO: add BSD and WIN32
IF(LINUX)
    SET(OONF_SUBSYSTEMS_SRCS ${OONF_SUBSYSTEMS_SRCS}
                             os_generic/os_net_generic_configsocket.c
                             os_generic/os_net_generic_getsocket.c
                             os_generic/os_net_generic_join_mcast.c
                             os_generic/os_net_generic_set_nonblocking.c
                             os_generic/os_routing_generic_rt_to_string.c
                             os_linux/os_clock_linux.c
                             os_linux/os_net_linux.c
                             os_linux/os_system_linux.c
                             os_linux/os_routing_linux.c
                             )
    SET(OONF_SUBSYSTEMS_INCLUDES ${OONF_SUBSYSTEMS_INCLUDES}
                             os_linux/os_clock_linux.h
                             os_linux/os_net_linux.h
                             os_linux/os_system_linux.h
                             os_linux/os_routing_linux.h
                             )
ENDIF(LINUX)

SET(linkto_internal oonf_common oonf_config oonf_rfc5444 oonf_core)
oonf_create_library("subsystems" "${OONF_SUBSYSTEMS_SRCS}" "${OONF_SUBSYSTEMS_INCLUDES}" "${linkto_internal}" "")

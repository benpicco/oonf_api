SET(OONF_SUBSYSTEMS_SRCS olsr_class.c
                         olsr_clock.c
                         olsr_duplicate_set.c
                         olsr_http.c
                         olsr_interface.c
                         olsr_packet_socket.c
                         olsr_rfc5444.c
                         olsr_socket.c
                         olsr_stream_socket.c
                         olsr_telnet.c
                         olsr_timer.c
                         )

SET(OONF_SUBSYSTEMS_INCLUDES olsr_class.h
                             olsr_clock.h
                             olsr_duplicate_set.h
                             olsr_http.h
                             olsr_interface.h
                             olsr_packet_socket.h
                             olsr_rfc5444.h
                             olsr_socket.h
                             olsr_stream_socket.h
                             olsr_telnet.h
                             olsr_timer.h
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
                             os_generic/os_net_generic_sendto.c
                             os_generic/os_net_generic_set_nonblocking.c
                             os_generic/os_routing_generic_rt_to_string.c
                             os_linux/os_net_linux.c
                             os_linux/os_system_linux.c
                             os_linux/os_routing_linux.c
                             )
    SET(OONF_SUBSYSTEMS_INCLUDES ${OONF_SUBSYSTEMS_INCLUDES}
                             os_linux/os_net_linux.h
                             os_linux/os_system_linux.h
                             os_linux/os_routing_linux.h
                             )
ENDIF(LINUX)

oonf_create_library("subsystems" "${OONF_SUBSYSTEMS_SRCS}" "${OONF_SUBSYSTEMS_INCLUDES}" "")

# prepare packetbb API configuration
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/pbb_api_config.h.in ${CMAKE_BINARY_DIR}/packetbb/pbb_api_config.h)

SET(OONF_PACKETBB_SRCS pbb_context.c
                       pbb_msg_generator.c
                       pbb_pkt_generator.c
                       pbb_reader.c
                       pbb_tlv_writer.c
                       pbb_writer.c
                       pbb_conversion.c
                       pbb_iana.c
                       pbb_print.c)

SET(OONF_PACKETBB_INCLUDES pbb_context.h
                           pbb_reader.h
                           pbb_tlv_writer.h
                           pbb_writer.h
                           pbb_conversion.h
                           pbb_iana.h
                           pbb_print.h)

oonf_create_library("packetbb" "${OONF_PACKETBB_SRCS}" "${OONF_PACKETBB_INCLUDES}" false)

# prepare rfc5444 API configuration
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/rfc5444_api_config.h.in ${CMAKE_BINARY_DIR}/rfc5444/rfc5444_api_config.h)

SET(OONF_RFC5444_SRCS rfc5444_context.c
                       rfc5444_msg_generator.c
                       rfc5444_pkt_generator.c
                       rfc5444_reader.c
                       rfc5444_tlv_writer.c
                       rfc5444_writer.c
                       rfc5444_conversion.c
                       rfc5444_iana.c
                       rfc5444_print.c)

SET(OONF_RFC5444_INCLUDES rfc5444_context.h
                           rfc5444_reader.h
                           rfc5444_tlv_writer.h
                           rfc5444_writer.h
                           rfc5444_conversion.h
                           rfc5444_iana.h
                           rfc5444_print.h)

oonf_create_library("rfc5444" "${OONF_RFC5444_SRCS}" "${OONF_RFC5444_INCLUDES}" false)

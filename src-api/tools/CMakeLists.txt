# make sure that olsr_libdata is rebuild everytimes 

SET(OONF_TOOLS_SRCS olsr_cfg.c
                    olsr_duplicate_set.c
                    olsr_http.c
                    olsr_logging_cfg.c
                    olsr_rfc5444.c
                    olsr_telnet.c)

SET(OONF_TOOLS_INCLUDES olsr_cfg.h
			olsr_duplicate_set.h
                        olsr_http.h
                        olsr_logging_cfg.h
                        olsr_rfc5444.h
                        olsr_telnet.h
                        )

oonf_create_library("tools" "${OONF_TOOLS_SRCS}" "${OONF_TOOLS_INCLUDES}" "rt")

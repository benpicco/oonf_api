SET(OONF_COMMON_SRCS  autobuf.c
                      avl_comp.c
                      avl.c
                      daemonize.c
                      netaddr.c
                      netaddr_acl.c
                      string.c
                      template.c)

SET(OONF_COMMON_INCLUDES autobuf.h
                         avl_comp.h
                         avl.h
                         common_types.h
                         container_of.h
                         daemonize.h
                         list.h
                         netaddr.h
                         netaddr_acl.h
                         string.h
                         template.h)

oonf_create_library("common" "${OONF_COMMON_SRCS}" "${OONF_COMMON_INCLUDES}" false)

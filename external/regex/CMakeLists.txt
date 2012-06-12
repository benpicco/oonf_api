SET(OONF_REGEX_SRCS regcomp.c
                    regerror.c
                    regexec.c
                    regfree.c)

ADD_LIBRARY(oonf_static_regex ${OONF_REGEX_SRCS})
ADD_LIBRARY(oonf_regex SHARED ${OONF_REGEX_SRCS})

IF(WIN32)
    TARGET_LINK_LIBRARIES(oonf_static_regex oonf_static_common)
    TARGET_LINK_LIBRARIES(oonf_regex oonf_common)

    TARGET_LINK_LIBRARIES(oonf_static_regex ws2_32 iphlpapi)
    TARGET_LINK_LIBRARIES(oonf_regex ws2_32 iphlpapi)
ENDIF(WIN32)

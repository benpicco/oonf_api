###########################
#### API configuration ####
###########################

# set CMAKE build type for api and plugins
# (Debug, Release, MinSizeRel)
set (CMAKE_BUILD_TYPE Debug)

# maximum logging level
set(OONF_MAX_LOGGING_LEVEL debug CACHE STRING "Maximum logging level compiled into OONF API (none, warn, info, debug)")

# name of the libnl library
set (OONF_LIBNL nl CACHE STRING "Library used for netlink operations")

####################################
#### RFC 5444 API configuration ####
####################################

# disallow the consumer to drop a tlv context
set (RFC5444_DISALLOW_CONSUMER_CONTEXT_DROP false CACHE BOOL "disallow the consumer to drop a tlv context")

# activate assets() to check state of the pbb writer
# and prevent calling functions at the wrong time
set (RFC5444_WRITER_STATE_MACHINE true CACHE BOOL "activate writer-statemachine")

# activate several unnecessary cleanup operations
# that make debugging the API easier
set (RFC5444_DEBUG_CLEANUP true CACHE BOOL "additional cleanup operations to simplify debugging")

# activate rfc5444 address-block compression
set (RFC5444_DO_ADDR_COMPRESSION true CACHE BOOL "use rfc5444 address compression")

# set to true to clear all bits in an address which are not included
# in the subnet mask
# set this to false to make interop tests!
set (RFC5444_CLEAR_ADDRESS_POSTFIX false CACHE BOOL "clear host bits of subnet addresses")

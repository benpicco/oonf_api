# find libnl

INCLUDE(FindPkgConfig)

pkg_check_modules(LIBNL3 libnl-tiny)
if (NOT LIBNL3_FOUND)
    pkg_check_modules(LIBNL3 REQUIRED libnl-3.0)
endif(NOT LIBNL3_FOUND)

include_directories(${LIBNL3_INCLUDE_DIRS})
link_directories(${LIBNL3_LIBRARY_DIRS})

# set library name
SET (source "nl80211_listener.c")

message("Using '${LIBNL3_LIBRARIES}' for nl80211 access")

# use generic plugin maker
oonf_create_plugin("nl80211_listener" "${source}" "" "${LIBNL3_LIBRARIES}")

# add subdirectories
add_subdirectory(cfgparser_compact)
add_subdirectory(cfgio_file)
add_subdirectory(httptelnet)
add_subdirectory(layer2_viewer)
add_subdirectory(plugin_controller)
add_subdirectory(remotecontrol)

# linux specific plugins
IF (LINUX)
    add_subdirectory(nl80211_listener)
ENDIF (LINUX)

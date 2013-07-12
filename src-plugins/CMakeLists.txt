# add subdirectories
add_subdirectory(cfgparser_compact)
add_subdirectory(cfgio_file)
add_subdirectory(httptelnet)
add_subdirectory(layer2_viewer)
add_subdirectory(link_config)
add_subdirectory(plugin_controller)
add_subdirectory(remotecontrol)

# linux specific plugins
IF (LINUX)
    add_subdirectory(nl80211_listener)
    add_subdirectory(eth_listener)
ENDIF (LINUX)

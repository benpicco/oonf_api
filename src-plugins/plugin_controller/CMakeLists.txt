# set library parameters
SET (source "plugin_controller.c")

# use generic plugin maker
oonf_create_plugin("plugin_controller" ${source} "" false)

# set library parameters
SET (source "remotecontrol.c")

# use generic plugin maker
oonf_create_plugin("remotecontrol" ${source} "" "")

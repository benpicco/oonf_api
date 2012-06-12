# set library parameters
SET (source "src/remotecontrol.c")

# use generic plugin maker
oonf_create_plugin("remotecontrol" ${source} false false)

# set library name
SET (source "layer2_viewer.c")

# use generic plugin maker
oonf_create_plugin("layer2_viewer" "${source}" "" "")

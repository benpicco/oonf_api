# set library name
SET (source "link_config.c")

# use generic plugin maker
oonf_create_plugin("link_config" "${source}" "" "")

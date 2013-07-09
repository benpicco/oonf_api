# set library name
SET (source "eth_listener.c")

# use generic plugin maker
oonf_create_plugin("eth_listener" "${source}" "" "")

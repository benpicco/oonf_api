# set library parameters
SET (source "src/httptelnet.c")

# use generic plugin maker
oonf_create_plugin("httptelnet" ${source} false false)

# set library parameters
SET (source "httptelnet.c")

# use generic plugin maker
oonf_create_plugin("httptelnet" ${source} "" false)

# set library parameters
SET (source "cfgparser_compact.c")

# use generic plugin maker
oonf_create_plugin("cfgparser_compact" ${source} "" "")

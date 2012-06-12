# set library parameters
SET (source "src/cfgparser_compact.c")
SET (include false)
SET (linkto false)

# use generic plugin maker
oonf_create_plugin("cfgparser_compact" ${source} false false)

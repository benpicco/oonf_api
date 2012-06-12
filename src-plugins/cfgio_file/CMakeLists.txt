# set library parameters
SET (source "src/cfgio_file.c")

# use generic plugin maker
oonf_create_plugin("cfgio_file" ${source} false false)

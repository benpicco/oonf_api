#!/bin/cmake

# look for git executable 
find_program(found_git git)

SET(OONF_LIB_GIT "cannot read git repository")

IF(NOT ${found_git} STREQUAL "found_git-NOTFOUND" AND EXISTS ${GIT})
    # everything is fine, read commit and diff stat
    execute_process(COMMAND git describe --always --long --tags --dirty
        OUTPUT_VARIABLE OONF_LIB_GIT OUTPUT_STRIP_TRAILING_WHITESPACE)
ENDIF()

# create builddata file
configure_file (${SRC} ${DST})

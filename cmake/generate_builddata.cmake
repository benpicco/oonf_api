#!/bin/cmake

# look for git executable 
find_program(found_git git)

SET(OONF_LIB_GIT "cannot read git repository")
SET(OONF_LIB_CHANGE "")

IF(NOT ${found_git} STREQUAL "found_git-NOTFOUND" AND EXISTS ${GIT})
    # everything is fine, read commit and diff stat
    execute_process(COMMAND git describe --always --long --tags
        OUTPUT_VARIABLE OONF_LIB_GIT OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND git diff --shortstat HEAD ./src-api/ ./src-plugins/ ./cmake ./external ./CMakeLists.txt
        OUTPUT_VARIABLE OONF_LIB_CHANGE OUTPUT_STRIP_TRAILING_WHITESPACE)
ENDIF()

# create builddata file
configure_file (${SRC} ${DST})

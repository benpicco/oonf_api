#!/bin/cmake

# look for git executable 
find_program(found_git git)

IF(NOT ${found_git} STREQUAL "found_git-NOTFOUND")
	# get git description WITHOUT dirty flag
	execute_process(COMMAND git describe --always --long --tags --match "v[0-9]*"
        	OUTPUT_VARIABLE OONF_LIB_GIT OUTPUT_STRIP_TRAILING_WHITESPACE)
    	
    	# extract version number
    	STRING(REGEX REPLACE "^v(.*)-[^-]*-[^-]*$" "\\1" OONF_VERSION ${OONF_LIB_GIT})
    	message ("Library version: ${OONF_VERSION}")
ELSE()
	message (FATAL_ERROR "Git not found, cannot get library version") 
ENDIF()

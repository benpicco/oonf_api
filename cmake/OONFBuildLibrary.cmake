# generic oonf library creation

function (oonf_internal_create_plugin prefix libname source include link_internal linkto_external)
    add_library(${prefix}_${libname} SHARED ${source})
    add_library(${prefix}_static_${libname} STATIC ${source})

    if(WIN32)
        target_link_libraries(${prefix}_${libname} ws2_32 iphlpapi)
    endif(WIN32)

    set_target_properties(${prefix}_${libname} PROPERTIES SOVERSION ${OONF_VERSION})

    if (linkto_internal)
        target_link_libraries(${prefix}_${libname} ${linkto_internal})
    endif (linkto_internal)
    if (linkto_external)
        target_link_libraries(${prefix}_${libname} ${linkto_external})
        target_link_libraries(${prefix}_static_${libname} ${linkto_external})
    endif (linkto_external)
    
    foreach(inc ${include})
        get_filename_component(path "${inc}" PATH)
        
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${inc}")
            install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/${inc}
                     DESTINATION ${INSTALL_INCLUDE_DIR}/${libname}/${path})
        ELSE (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${inc}")
            install (FILES ${CMAKE_BINARY_DIR}/${libname}/${inc}
                     DESTINATION ${INSTALL_INCLUDE_DIR}/${libname}/${path})
        ENDIF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${inc}")
    endforeach(inc)
endfunction (oonf_internal_create_plugin)

function (oonf_create_library libname source include linkto_internal linkto_external)
    oonf_internal_create_plugin("oonf" "${libname}" "${source}" "${include}" "${linkto_internal}" "${linkto_external}")
    
    install (TARGETS oonf_${libname}
        # IMPORTANT: Add the library to the "export-set"
        EXPORT OONFLibraryDepends
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
            COMPONENT dev)

    install (TARGETS oonf_static_${libname}
        # IMPORTANT: Add the static library to the "export-set"
        EXPORT OONFLibraryDepends
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT stlib
            COMPONENT dev)
    #
   
    get_property (targets GLOBAL PROPERTY OONF_TARGETS)
    SET (targets ${targets} oonf_${libname} oonf_static_${libname})
    set_property(GLOBAL PROPERTY OONF_TARGETS "${targets}") 
    
#    export (TARGETS oonf_${libname} oonf_static_${libname} ${linkto_internal}
#            FILE "${PROJECT_BINARY_DIR}/OONFLibraryDepends_${libname}.cmake")
endfunction (oonf_create_library)

function (oonf_create_plugin libname source include linkto_external)
    SET (linkto_internal oonf_subsystems oonf_core oonf_config oonf_rfc5444 oonf_common)
    
    oonf_create_library("${libname}" "${source}" "${include}" "${linkto_internal}" "${linkto_external}")
    
    set_source_files_properties(${source} PROPERTIES COMPILE_FLAGS "-DPLUGIN_FULLNAME=${libname}")
endfunction (oonf_create_plugin)

function (oonf_create_app_plugin libname source include linkto_external)
    SET (linkto_internal oonf_subsystems oonf_core oonf_config oonf_rfc5444 oonf_common)
    
    oonf_internal_create_plugin("${OONF_APP_LIBPREFIX}" "${libname}" "${source}" "${include}" "${linkto_internal}" "${linkto_external}")
    
    set_source_files_properties(${source} PROPERTIES COMPILE_FLAGS "-DPLUGIN_FULLNAME=${libname}")
    
    install (TARGETS ${OONF_APP_LIBPREFIX}_${libname}
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/${libname}"
            COMPONENT dev)

    install (TARGETS ${OONF_APP_LIBPREFIX}_static_${libname}
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT stlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/${libname}"
            COMPONENT dev)
endfunction (oonf_create_app_plugin)

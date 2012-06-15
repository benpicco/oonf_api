# generic oonf library creation

function (oonf_internal_create_plugin prefix libname source include linkto)
    ADD_LIBRARY(${prefix}_${libname} SHARED ${source})
    ADD_LIBRARY(${prefix}_static_${libname} STATIC ${source})

    IF(WIN32)
        TARGET_LINK_LIBRARIES(${prefix}_${libname} ws2_32 iphlpapi)
    ENDIF(WIN32)

    SET_TARGET_PROPERTIES(${prefix}_${libname} PROPERTIES SOVERSION ${OONF_VERSION})

    IF (linkto)
        TARGET_LINK_LIBRARIES(${prefix}_${libname} ${linkto})
        TARGET_LINK_LIBRARIES(${prefix}_static_${libname} ${linkto})
    ENDIF (linkto)
    
    IF (include)
        set_target_properties(${prefix}_${libname} PROPERTIES
            PUBLIC_HEADER "${include}")
  
        set_target_properties(${prefix}_static_${libname} PROPERTIES
            PUBLIC_HEADER "${include}")
    ENDIF (include)
endfunction (oonf_internal_create_plugin)

function (oonf_create_library libname source include linkto)
    oonf_internal_create_plugin(oonf "${libname}" "${source}" "${include}" "${linkto}")
    
    # Add all target to the build-tree export set
    export(TARGETS oonf_${libname} oonf_static_${libname}
        APPEND FILE "${PROJECT_BINARY_DIR}/OONFLibraryDepends.cmake")
    
    install(TARGETS oonf_${libname}
        # IMPORTANT: Add the library to the "export-set"
        EXPORT OONFLibraryDepends
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/${libname}"
            COMPONENT dev)

    install(TARGETS oonf_static_${libname}
        # IMPORTANT: Add the static library to the "export-set"
        EXPORT OONFLibraryDepends
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT stlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/${libname}"
            COMPONENT dev)
endfunction (oonf_create_library)

function (oonf_create_plugin libname source include linkto)
    oonf_create_library("${libname}" "${source}" "${include}" "${linkto}")
    
    SET_SOURCE_FILES_PROPERTIES(${source} PROPERTIES COMPILE_FLAGS "-DPLUGIN_FULLNAME=${libname}")
endfunction (oonf_create_plugin)

function (oonf_create_app_plugin libname source include linkto)
    oonf_internal_create_plugin("${OONF_APP_LIBPREFIX}" "${libname}" "${source}" "${include}" "${linkto}")
    
    SET_SOURCE_FILES_PROPERTIES(${source} PROPERTIES COMPILE_FLAGS "-DPLUGIN_FULLNAME=${libname}")
    
    message ("install(TARGETS ${OONF_APP_LIBPREFIX}_${libname}
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/${libname}"
            COMPONENT dev)")
    
    install(TARGETS ${OONF_APP_LIBPREFIX}_${libname}
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/${libname}"
            COMPONENT dev)

    install(TARGETS ${OONF_APP_LIBPREFIX}_static_${libname}
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT stlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/${libname}"
            COMPONENT dev)
endfunction (oonf_create_app_plugin)

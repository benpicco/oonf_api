function(compile_rfc5444_test executable source)
    # create executable
    ADD_EXECUTABLE(${executable} ${source})

    TARGET_LINK_LIBRARIES(${executable} oonf_rfc5444)
    TARGET_LINK_LIBRARIES(${executable} oonf_common)
    TARGET_LINK_LIBRARIES(${executable} static_cunit)

    # link regex for windows and android
    IF (WIN32 OR ANDROID)
        TARGET_LINK_LIBRARIES(${executable} oonf_regex)
    ENDIF(WIN32 OR ANDROID)

    # link extra win32 libs
    IF(WIN32)
        SET_TARGET_PROPERTIES(${executable} PROPERTIES ENABLE_EXPORTS true)
        TARGET_LINK_LIBRARIES(${executable} ws2_32 iphlpapi)
    ENDIF(WIN32)
endfunction(compile_rfc5444_test)

set(TESTS test_rfc5444_reader_blockcb
          test_rfc5444_reader_dropcontext
          test_rfc5444_writer_fragmentation
          test_rfc5444_writer_ifspecific
          test_rfc5444_writer_mandatory
          test_rfc5444_conversion)

foreach(TEST ${TESTS})
    compile_rfc5444_test(${TEST} ${TEST}.c)
    ADD_TEST(NAME ${TEST} COMMAND ${TEST})
endforeach(TEST)

add_subdirectory(interop2010)

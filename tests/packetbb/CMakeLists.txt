function(compile_packetbb_test executable source)
    # create executable
    ADD_EXECUTABLE(${executable} ${source})

    TARGET_LINK_LIBRARIES(${executable} oonf_packetbb)
    TARGET_LINK_LIBRARIES(${executable} oonf_common)

    # link regex for windows and android
    IF (WIN32 OR ANDROID)
        TARGET_LINK_LIBRARIES(${executable} oonf_regex)
    ENDIF(WIN32 OR ANDROID)

    # link extra win32 libs
    IF(WIN32)
        SET_TARGET_PROPERTIES(${executable} PROPERTIES ENABLE_EXPORTS true)
        TARGET_LINK_LIBRARIES(${executable} ws2_32 iphlpapi)
    ENDIF(WIN32)
endfunction(compile_packetbb_test)

set(TESTS test_packetbb_reader_blockcb
          test_packetbb_reader_dropcontext
          test_packetbb_writer_fragmentation
          test_packetbb_writer_ifspecific
          test_packetbb_conversion)

foreach(TEST ${TESTS})
    compile_packetbb_test(${TEST} ${TEST}.c)
    ADD_TEST(NAME ${TEST} COMMAND ${TEST})
endforeach(TEST)

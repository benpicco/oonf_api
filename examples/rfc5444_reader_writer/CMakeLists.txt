# create executable
ADD_EXECUTABLE(example_rfc5444_reader_writer reader.c writer.c main.c)

TARGET_LINK_LIBRARIES(example_rfc5444_reader_writer oonf_rfc5444)
TARGET_LINK_LIBRARIES(example_rfc5444_reader_writer oonf_common)

# link regex for windows and android
IF (WIN32 OR ANDROID)
    TARGET_LINK_LIBRARIES(example_rfc5444_reader_writer oonf_regex)
ENDIF(WIN32 OR ANDROID)

# link extra win32 libs
IF(WIN32)
    SET_TARGET_PROPERTIES(example_rfc5444_reader_writer PROPERTIES ENABLE_EXPORTS true)
    TARGET_LINK_LIBRARIES(example_rfc5444_reader_writer ws2_32 iphlpapi)
ENDIF(WIN32)

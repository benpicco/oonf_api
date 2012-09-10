set(TEST test_rfc5444_interop2010_01.c
         test_rfc5444_interop2010_02.c
         test_rfc5444_interop2010_03.c
         test_rfc5444_interop2010_04.c
         test_rfc5444_interop2010_05.c
         test_rfc5444_interop2010_06.c
         test_rfc5444_interop2010_07.c
         test_rfc5444_interop2010_08.c
         test_rfc5444_interop2010_09.c
         test_rfc5444_interop2010_10.c
         test_rfc5444_interop2010_11.c
         test_rfc5444_interop2010_12.c
         test_rfc5444_interop2010_13.c
         test_rfc5444_interop2010_14.c
         test_rfc5444_interop2010_15.c
         test_rfc5444_interop2010_16.c
         test_rfc5444_interop2010_17.c
         test_rfc5444_interop2010_18.c
         test_rfc5444_interop2010_19.c
         test_rfc5444_interop2010_20.c
         test_rfc5444_interop2010_21.c
         test_rfc5444_interop2010_22.c
         test_rfc5444_interop2010_23.c
         test_rfc5444_interop2010_24.c
         test_rfc5444_interop2010_25.c
         test_rfc5444_interop2010_26.c
         test_rfc5444_interop2010_27.c
         test_rfc5444_interop2010_28.c

         test_rfc5444_interop2010_29.c
         test_rfc5444_interop2010_30.c
         test_rfc5444_interop2010_31.c
         test_rfc5444_interop2010_32.c
         test_rfc5444_interop2010_33.c
         test_rfc5444_interop2010_34.c
         test_rfc5444_interop2010_35.c
         
         test_rfc5444_interop2010_36.c
         
         test_rfc5444_interop2010_38.c
         )

# create executable
ADD_EXECUTABLE(test_rfc5444_interop2010 ${TEST} test_rfc5444_interop2010.c)

TARGET_LINK_LIBRARIES(test_rfc5444_interop2010 oonf_rfc5444)
TARGET_LINK_LIBRARIES(test_rfc5444_interop2010 oonf_common)
TARGET_LINK_LIBRARIES(test_rfc5444_interop2010 static_cunit)

# link regex for windows and android
IF (WIN32 OR ANDROID)
    TARGET_LINK_LIBRARIES(test_rfc5444_interop2010 oonf_regex)
ENDIF(WIN32 OR ANDROID)

# link extra win32 libs
IF(WIN32)
    SET_TARGET_PROPERTIES(test_rfc5444_interop2010 PROPERTIES ENABLE_EXPORTS true)
    TARGET_LINK_LIBRARIES(test_rfc5444_interop2010 ws2_32 iphlpapi)
ENDIF(WIN32)

ADD_TEST(NAME test_rfc5444_interop2010 COMMAND test_rfc5444_interop2010)

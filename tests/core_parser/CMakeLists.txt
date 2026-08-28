add_executable(core_parser_tests
    BitReaderTest.cpp
    ExpGolombTest.cpp
    NaluExtractorTest.cpp
    H264SpsPpsParserTest.cpp
    H265SpsPpsParserTest.cpp
    SliceHeaderParserTest.cpp
)

target_link_libraries(core_parser_tests PRIVATE
    core_parser
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(core_parser_tests)

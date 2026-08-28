# Layer 1: pure C++ parser core. Zero third-party dependencies by design —
# do not add find_package()/target_link_libraries() to anything other than
# the standard library here. Layer 2/3 depend on this target, never the
# other way around.
add_library(core_parser STATIC
    src/NaluTypes.cpp
    src/BitReader.cpp
    src/ExpGolomb.cpp
    src/NaluExtractor.cpp
    src/H264SpsPpsParser.cpp
    src/H265SpsPpsParser.cpp
    src/SliceHeaderParser.cpp
)

target_include_directories(core_parser PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(core_parser PUBLIC cxx_std_17)

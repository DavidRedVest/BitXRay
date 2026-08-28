#include "core_parser/NaluExtractor.h"

#include <gtest/gtest.h>

using namespace bitxray;

namespace {

std::vector<uint8_t> concat(std::initializer_list<std::vector<uint8_t>> chunks) {
    std::vector<uint8_t> out;
    for (const auto& c : chunks) out.insert(out.end(), c.begin(), c.end());
    return out;
}

} // namespace

TEST(NaluExtractor, FindsNalusWithFourByteStartCodes) {
    // SPS (type 7): nal header 0x67 -> ref_idc=3, type=7.
    const std::vector<uint8_t> sps = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x0a};
    // PPS (type 8): nal header 0x68.
    const std::vector<uint8_t> pps = {0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x3c, 0x80};
    const auto buf = concat({sps, pps});

    const auto nalus = extractNalus(buf.data(), buf.size(), Codec::H264);
    ASSERT_EQ(nalus.size(), 2u);

    EXPECT_EQ(nalus[0].offset, 4u);
    EXPECT_EQ(nalus[0].length, 4u);
    EXPECT_EQ(nalus[0].startCodeLen, 4);
    EXPECT_EQ(nalus[0].naluType, 7);
    EXPECT_TRUE(nalus[0].isParameterSet());

    EXPECT_EQ(nalus[1].offset, 12u);
    EXPECT_EQ(nalus[1].length, 4u);
    EXPECT_EQ(nalus[1].naluType, 8);
}

TEST(NaluExtractor, FindsNalusWithThreeByteStartCodes) {
    const std::vector<uint8_t> a = {0x00, 0x00, 0x01, 0x67, 0xAA};
    const std::vector<uint8_t> b = {0x00, 0x00, 0x01, 0x68, 0xBB};
    const auto buf = concat({a, b});

    const auto nalus = extractNalus(buf.data(), buf.size(), Codec::H264);
    ASSERT_EQ(nalus.size(), 2u);
    EXPECT_EQ(nalus[0].startCodeLen, 3);
    EXPECT_EQ(nalus[0].offset, 3u);
    EXPECT_EQ(nalus[0].length, 2u);
    EXPECT_EQ(nalus[1].offset, 8u);
}

TEST(NaluExtractor, HandlesMixedThreeAndFourByteStartCodes) {
    const std::vector<uint8_t> buf = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0xAA, 0xBB, // 4-byte start code
        0x00, 0x00, 0x01, 0x68, 0xCC,             // 3-byte start code
    };
    const auto nalus = extractNalus(buf.data(), buf.size(), Codec::H264);
    ASSERT_EQ(nalus.size(), 2u);
    EXPECT_EQ(nalus[0].length, 3u);
    EXPECT_EQ(nalus[1].length, 2u);
}

TEST(NaluExtractor, LastNaluRunsToEndOfBufferEvenIfTruncated) {
    const std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01, 0x65, 0x01, 0x02, 0x03};
    const auto nalus = extractNalus(buf.data(), buf.size(), Codec::H264);
    ASSERT_EQ(nalus.size(), 1u);
    EXPECT_EQ(nalus[0].offset, 4u);
    EXPECT_EQ(nalus[0].length, 4u);
}

TEST(NaluExtractor, ReturnsEmptyForBufferWithNoStartCode) {
    const std::vector<uint8_t> buf = {0x01, 0x02, 0x03, 0x04};
    const auto nalus = extractNalus(buf.data(), buf.size(), Codec::H264);
    EXPECT_TRUE(nalus.empty());
}

TEST(NaluExtractor, DecodesH265TwoByteNaluHeader) {
    // VPS (type 32): header bytes 0x40 0x01 -> (0x40>>1)&0x3F = 32.
    const std::vector<uint8_t> vps = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0xAA};
    // SPS (type 33): header bytes 0x42 0x01.
    const std::vector<uint8_t> sps = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0xBB};
    const auto buf = concat({vps, sps});

    const auto nalus = extractNalus(buf.data(), buf.size(), Codec::H265);
    ASSERT_EQ(nalus.size(), 2u);
    EXPECT_EQ(nalus[0].naluType, 32);
    EXPECT_EQ(nalus[1].naluType, 33);
    EXPECT_TRUE(nalus[1].isParameterSet());
}

TEST(DetectCodec, IdentifiesH264FromSpsHeader) {
    const std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x0a};
    EXPECT_EQ(detectCodec(buf.data(), buf.size()), Codec::H264);
}

TEST(DetectCodec, IdentifiesH265FromSpsHeader) {
    const std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x04};
    EXPECT_EQ(detectCodec(buf.data(), buf.size()), Codec::H265);
}

TEST(DetectCodec, ReturnsUnknownWithoutParameterSets) {
    const std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01, 0x65, 0x01, 0x02};
    EXPECT_EQ(detectCodec(buf.data(), buf.size()), Codec::Unknown);
}

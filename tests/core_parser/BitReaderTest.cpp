#include "core_parser/BitReader.h"

#include <gtest/gtest.h>

using namespace bitxray;

TEST(BitReader, ReadsIndividualBitsMsbFirst) {
    const uint8_t data[] = {0b10110010};
    BitReader reader(data, sizeof(data));

    EXPECT_EQ(reader.readBit(), 1u);
    EXPECT_EQ(reader.readBit(), 0u);
    EXPECT_EQ(reader.readBit(), 1u);
    EXPECT_EQ(reader.readBit(), 1u);
    EXPECT_EQ(reader.readBit(), 0u);
    EXPECT_EQ(reader.readBit(), 0u);
    EXPECT_EQ(reader.readBit(), 1u);
    EXPECT_EQ(reader.readBit(), 0u);
}

TEST(BitReader, ReadsMultiBitFields) {
    const uint8_t data[] = {0xAB, 0xCD}; // 1010 1011 1100 1101
    BitReader reader(data, sizeof(data));

    EXPECT_EQ(reader.u(4), 0xAu);
    EXPECT_EQ(reader.u(4), 0xBu);
    EXPECT_EQ(reader.u(8), 0xCDu);
}

TEST(BitReader, TracksByteAlignmentAndPosition) {
    const uint8_t data[] = {0xFF, 0xFF};
    BitReader reader(data, sizeof(data));

    EXPECT_TRUE(reader.byteAligned());
    reader.u(3);
    EXPECT_FALSE(reader.byteAligned());
    EXPECT_EQ(reader.bitPosition(), 3u);
    reader.byteAlign();
    EXPECT_TRUE(reader.byteAligned());
    EXPECT_EQ(reader.bitPosition(), 8u);
    EXPECT_EQ(reader.bitsRemaining(), 8u);
}

TEST(BitReader, ThrowsOnOverrun) {
    const uint8_t data[] = {0xFF};
    BitReader reader(data, sizeof(data));
    reader.u(8);
    EXPECT_THROW(reader.readBit(), BitstreamOverrunError);
}

TEST(UnescapeRbsp, PassesThroughDataWithoutEmulationBytes) {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    const auto result = unescapeRbsp(data, sizeof(data));
    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 0x01);
    EXPECT_EQ(result[3], 0x04);
}

TEST(UnescapeRbsp, RemovesEmulationPreventionByte) {
    // 00 00 03 01 -> 00 00 01 ; 00 00 03 02 -> 00 00 02 ; 00 00 03 03 -> 00 00 03
    const uint8_t data[] = {0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x03, 0x02,
                             0x00, 0x00, 0x03, 0x03};
    const auto result = unescapeRbsp(data, sizeof(data));
    const std::vector<uint8_t> expected = {0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x03};
    EXPECT_EQ(result, expected);
}

TEST(UnescapeRbsp, DoesNotStripByteAfterLessThanTwoZeros) {
    // A lone 0x03 following a single zero byte is real data, not an
    // emulation-prevention byte, and must be preserved.
    const uint8_t data[] = {0x00, 0x03, 0x05};
    const auto result = unescapeRbsp(data, sizeof(data));
    const std::vector<uint8_t> expected = {0x00, 0x03, 0x05};
    EXPECT_EQ(result, expected);
}

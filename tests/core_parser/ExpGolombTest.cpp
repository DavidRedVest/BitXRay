#include "core_parser/ExpGolomb.h"

#include <gtest/gtest.h>

using namespace bitxray;

namespace {

uint32_t decodeUeFromBits(std::initializer_list<uint8_t> bits) {
    // Packs a list of individual 0/1 bits (MSB-first within each byte,
    // padded with trailing zero bits) into a byte buffer, then decodes one
    // ue(v) value from the front of it.
    std::vector<uint8_t> bytes((bits.size() + 7) / 8, 0);
    std::size_t i = 0;
    for (uint8_t bit : bits) {
        if (bit) {
            bytes[i / 8] |= (1u << (7 - (i % 8)));
        }
        ++i;
    }
    BitReader reader(bytes.data(), bytes.size());
    return readUe(reader);
}

} // namespace

TEST(ExpGolomb, DecodesUeCodeTable) {
    // Table 9-2 of the H.264/H.265 spec, first few code words.
    EXPECT_EQ(decodeUeFromBits({1}), 0u);
    EXPECT_EQ(decodeUeFromBits({0, 1, 0}), 1u);
    EXPECT_EQ(decodeUeFromBits({0, 1, 1}), 2u);
    EXPECT_EQ(decodeUeFromBits({0, 0, 1, 0, 0}), 3u);
    EXPECT_EQ(decodeUeFromBits({0, 0, 1, 0, 1}), 4u);
    EXPECT_EQ(decodeUeFromBits({0, 0, 1, 1, 0}), 5u);
    EXPECT_EQ(decodeUeFromBits({0, 0, 1, 1, 1}), 6u);
}

TEST(ExpGolomb, DecodesSeMappingFromUeCodeNum) {
    // se(v) mapping (spec Table 9-3): codeNum 0,1,2,3,4 -> 0,1,-1,2,-2.
    auto decodeSe = [](std::initializer_list<uint8_t> bits) {
        std::vector<uint8_t> bytes((bits.size() + 7) / 8, 0);
        std::size_t i = 0;
        for (uint8_t bit : bits) {
            if (bit) bytes[i / 8] |= (1u << (7 - (i % 8)));
            ++i;
        }
        BitReader reader(bytes.data(), bytes.size());
        return readSe(reader);
    };

    EXPECT_EQ(decodeSe({1}), 0);
    EXPECT_EQ(decodeSe({0, 1, 0}), 1);
    EXPECT_EQ(decodeSe({0, 1, 1}), -1);
    EXPECT_EQ(decodeSe({0, 0, 1, 0, 0}), 2);
    EXPECT_EQ(decodeSe({0, 0, 1, 0, 1}), -2);
}

TEST(ExpGolomb, ReadsConsecutiveValuesInSequence) {
    // ue(v)=0 ("1") followed by ue(v)=2 ("011") followed by ue(v)=1 ("010").
    const uint8_t data[] = {0b10110100}; // "1" "011" "010" then a trailing pad bit
    BitReader reader(data, sizeof(data));
    EXPECT_EQ(readUe(reader), 0u);
    EXPECT_EQ(readUe(reader), 2u);
    EXPECT_EQ(readUe(reader), 1u);
}

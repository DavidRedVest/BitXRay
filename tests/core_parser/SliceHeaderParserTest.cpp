#include "core_parser/SliceHeaderParser.h"

#include <gtest/gtest.h>

#include "core_parser/NaluTypes.h"

using namespace bitxray;

TEST(SliceHeaderParser, H264ClassifiesIFrame) {
    // first_mb_in_slice=ue(0)="1", slice_type=ue(2)="011" -> I (2 % 5 == 2).
    const uint8_t rbsp[] = {0b10110000};
    const auto type = parseH264SliceType(rbsp, sizeof(rbsp));
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(*type, SliceType::I);
}

TEST(SliceHeaderParser, H264ClassifiesPFrame) {
    // first_mb_in_slice=ue(0)="1", slice_type=ue(0)="1" -> P (0 % 5 == 0).
    const uint8_t rbsp[] = {0b11000000};
    const auto type = parseH264SliceType(rbsp, sizeof(rbsp));
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(*type, SliceType::P);
}

TEST(SliceHeaderParser, H264ClassifiesBFrame) {
    // first_mb_in_slice=ue(0)="1", slice_type=ue(1)="010" -> B (1 % 5 == 1).
    const uint8_t rbsp[] = {0b10100000};
    const auto type = parseH264SliceType(rbsp, sizeof(rbsp));
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(*type, SliceType::B);
}

TEST(SliceHeaderParser, H265ClassifiesIdrAsIFrame) {
    // first_slice_segment_in_pic_flag=1, IDR so no_output_of_prior_pics_flag
    // bit follows =0, slice_pic_parameter_set_id=ue(0)="1",
    // numExtraSliceHeaderBits=0, slice_type=ue(2)="011" -> I.
    // Bits: 1 0 1 011 -> 1010 11xx
    const uint8_t rbsp[] = {0b10101100};
    const auto type = parseH265SliceType(rbsp, sizeof(rbsp),
                                          static_cast<uint8_t>(H265NaluType::IdrWRadl),
                                          /*numExtraSliceHeaderBits=*/0);
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(*type, SliceType::I);
}

TEST(SliceHeaderParser, H265ClassifiesTrailAsPOrB) {
    // Non-IRAP (TRAIL_R): first_slice_segment_in_pic_flag=1 (no
    // no_output_of_prior_pics_flag bit), slice_pic_parameter_set_id=ue(0)="1",
    // slice_type=ue(1)="010" -> P (H.265 mapping: 0=B,1=P,2=I).
    // Bits: 1 1 010 -> 1101 0xxx
    const uint8_t rbsp[] = {0b11010000};
    const auto type = parseH265SliceType(rbsp, sizeof(rbsp),
                                          static_cast<uint8_t>(H265NaluType::TrailR),
                                          /*numExtraSliceHeaderBits=*/0);
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(*type, SliceType::P);
}

TEST(SliceHeaderParser, H265ReturnsNulloptForNonFirstSliceSegment) {
    // first_slice_segment_in_pic_flag=0 -> we deliberately bail rather than
    // guess, since slice_segment_address decoding needs SPS geometry.
    const uint8_t rbsp[] = {0b00000000};
    const auto type = parseH265SliceType(rbsp, sizeof(rbsp),
                                          static_cast<uint8_t>(H265NaluType::TrailR));
    EXPECT_FALSE(type.has_value());
}

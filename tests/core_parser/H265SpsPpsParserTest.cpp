#include "core_parser/H265SpsPpsParser.h"

#include <gtest/gtest.h>

#include "core_parser/BitReader.h"

using namespace bitxray;

namespace {

// Real SPS RBSP bytes (2-byte NAL header 0x42 0x01 already stripped),
// including emulation-prevention bytes as they appear on the wire, captured
// from a 176x144 stream encoded by the project's own local FFmpeg build
// (libx265, Range Extensions profile per `ffprobe` -- picked because the
// synthetic `testsrc` source defaults to a GBR color format under libx265).
const uint8_t kSampleSpsRbsp[] = {
    0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9e, 0x08, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x3c, 0x90, 0x02, 0xc4, 0x04, 0x8b, 0x2c, 0xac, 0xd2, 0x49,
    0x95, 0xe0, 0x2d, 0xc0, 0x80, 0x80, 0x01, 0x00, 0x00, 0x03, 0x00, 0x01,
    0x00, 0x00, 0x03, 0x00, 0x19, 0x08,
};

} // namespace

TEST(H265SpsPpsParser, ParsesRealCapturedSpsResolutionAndProfile) {
    const auto unescaped = unescapeRbsp(kSampleSpsRbsp, sizeof(kSampleSpsRbsp));
    const auto sps = parseH265Sps(unescaped.data(), unescaped.size());

    ASSERT_TRUE(sps.has_value());
    EXPECT_EQ(sps->maxSubLayersMinus1, 0);
    EXPECT_EQ(sps->ptl.generalProfileIdc, 4); // Range Extensions, confirmed via ffprobe
    EXPECT_EQ(sps->width, 176u);
    EXPECT_EQ(sps->height, 144u);
    EXPECT_EQ(sps->chromaFormatIdc, 3u); // 4:4:4, matches libx265's gbrp output here
}

TEST(H265SpsPpsParser, ReturnsNulloptForTooShortPayload) {
    const uint8_t tooShort[] = {0x01, 0x02};
    EXPECT_FALSE(parseH265Sps(tooShort, sizeof(tooShort)).has_value());
}

TEST(H265SpsPpsParser, ParsesPpsBasicFields) {
    // Hand-crafted minimal PPS bit sequence, see comment math in the plan:
    // pps_id=ue(0), sps_id=ue(0), dependent_slice_segments=0,
    // output_flag_present=1, num_extra_slice_header_bits=000,
    // sign_data_hiding=1, cabac_init_present=0,
    // num_ref_idx_l0_default_active_minus1=ue(0),
    // num_ref_idx_l1_default_active_minus1=ue(0), init_qp_minus26=se(0).
    const uint8_t ppsRbsp[] = {0b11010001, 0b01110000};
    const auto pps = parseH265Pps(ppsRbsp, sizeof(ppsRbsp));

    ASSERT_TRUE(pps.has_value());
    EXPECT_EQ(pps->picParameterSetId, 0u);
    EXPECT_EQ(pps->seqParameterSetId, 0u);
    EXPECT_FALSE(pps->dependentSliceSegmentsEnabledFlag);
    EXPECT_TRUE(pps->outputFlagPresentFlag);
    EXPECT_EQ(pps->numExtraSliceHeaderBits, 0);
    EXPECT_TRUE(pps->signDataHidingEnabledFlag);
    EXPECT_FALSE(pps->cabacInitPresentFlag);
    EXPECT_EQ(pps->initQpMinus26, 0);
}

#include "core_parser/H264SpsPpsParser.h"

#include <gtest/gtest.h>

#include "core_parser/BitReader.h"

using namespace bitxray;

namespace {

// Real SPS RBSP bytes (NAL header 0x27 already stripped) captured from a
// 176x144 stream encoded by the project's own local FFmpeg build
// (h264_videotoolbox, High profile). No emulation-prevention bytes happen
// to occur in this particular payload, but it's run through unescapeRbsp
// anyway since that's what the real parsing pipeline does.
const uint8_t kSampleSpsRbsp[] = {0x64, 0x00, 0x0d, 0xac, 0x56, 0x83, 0x04, 0xf8, 0x9d};

} // namespace

TEST(H264SpsPpsParser, ParsesRealCapturedSpsResolutionAndProfile) {
    const auto unescaped = unescapeRbsp(kSampleSpsRbsp, sizeof(kSampleSpsRbsp));
    const auto sps = parseH264Sps(unescaped.data(), unescaped.size());

    ASSERT_TRUE(sps.has_value());
    EXPECT_EQ(sps->profileIdc, 100); // High profile
    EXPECT_EQ(sps->width, 176u);
    EXPECT_EQ(sps->height, 144u);
    EXPECT_EQ(sps->chromaFormatIdc, 1u); // 4:2:0
    EXPECT_EQ(sps->bitDepthLumaMinus8, 0u);
    EXPECT_EQ(sps->bitDepthChromaMinus8, 0u);
    EXPECT_TRUE(sps->frameMbsOnlyFlag);
}

TEST(H264SpsPpsParser, ReturnsNulloptForTooShortPayload) {
    const uint8_t tooShort[] = {0x64, 0x00};
    EXPECT_FALSE(parseH264Sps(tooShort, sizeof(tooShort)).has_value());
}

TEST(H264SpsPpsParser, ParsesPpsBasicFields) {
    // Hand-crafted minimal PPS: pic_parameter_set_id=ue(0)="1",
    // seq_parameter_set_id=ue(0)="1", entropy_coding_mode_flag=1,
    // bottom_field_pic_order_in_frame_present_flag=0,
    // num_slice_groups_minus1=ue(0)="1" (single slice group, parsing continues),
    // num_ref_idx_l0_default_active_minus1=ue(0)="1",
    // num_ref_idx_l1_default_active_minus1=ue(0)="1",
    // weighted_pred_flag=0, weighted_bipred_idc=u(2)=00,
    // pic_init_qp_minus26=se(0)="1", pic_init_qs_minus26=se(0)="1",
    // chroma_qp_index_offset=se(0)="1",
    // deblocking_filter_control_present_flag=1,
    // constrained_intra_pred_flag=0,
    // redundant_pic_cnt_present_flag=0.
    // Bit sequence: 1 1 1 0 1 1 1 0 00 1 1 1 1 0 0 -> pad to bytes.
    // = 1110 1110 0011 1100  (16 bits = 2 bytes)
    const uint8_t ppsRbsp[] = {0b11101110, 0b00111100};
    const auto pps = parseH264Pps(ppsRbsp, sizeof(ppsRbsp));

    ASSERT_TRUE(pps.has_value());
    EXPECT_EQ(pps->picParameterSetId, 0u);
    EXPECT_EQ(pps->seqParameterSetId, 0u);
    EXPECT_TRUE(pps->entropyCodingModeFlag);
    EXPECT_FALSE(pps->bottomFieldPicOrderInFramePresentFlag);
    EXPECT_EQ(pps->numSliceGroupsMinus1, 0u);
    EXPECT_EQ(pps->numRefIdxL0DefaultActiveMinus1, 0u);
    EXPECT_EQ(pps->numRefIdxL1DefaultActiveMinus1, 0u);
    EXPECT_FALSE(pps->weightedPredFlag);
    EXPECT_EQ(pps->weightedBipredIdc, 0);
    EXPECT_TRUE(pps->deblockingFilterControlPresentFlag);
    EXPECT_FALSE(pps->constrainedIntraPredFlag);
    EXPECT_FALSE(pps->redundantPicCntPresentFlag);
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "core_parser/H264SpsPpsParser.h"
#include "core_parser/H265SpsPpsParser.h"

namespace bitxray {

enum class SliceType {
    P,
    B,
    I,
    SP, // H.264 only
    SI, // H.264 only
};

// Reads just enough of an H.264 slice header (first_mb_in_slice, slice_type)
// to classify the slice for I/P/B row highlighting in the NALU list.
// `rbsp` must have the 1-byte NAL header stripped and be emulation-prevention
// unescaped already.
[[nodiscard]] std::optional<SliceType> parseH264SliceType(const uint8_t* rbsp, std::size_t size);

// Reads just enough of an H.265 slice segment header to classify the slice.
// Only handles the common "first (and typically only) slice segment in the
// picture" case, since decoding a dependent/non-first slice segment's
// address requires SPS-derived CTB geometry that isn't needed for anything
// else in this viewer; returns std::nullopt for that case rather than
// guessing. `numExtraSliceHeaderBits` comes from the slice's active PPS
// (defaults to 0, the overwhelmingly common case, if the PPS isn't known
// yet). `rbsp` preconditions match parseH264SliceType.
[[nodiscard]] std::optional<SliceType> parseH265SliceType(const uint8_t* rbsp, std::size_t size,
                                                           uint8_t naluType,
                                                           uint8_t numExtraSliceHeaderBits = 0);

// Full H.264 slice_header() breakdown, for the syntax-tree detail view
// (mirrors what tools like H264BSAnalyzer show). Needs the slice's active
// SPS/PPS to know field widths and which optional sections apply — this is
// why it's a separate, heavier function from parseH264SliceType. Correctly
// parses/skips ref_pic_list_modification(), pred_weight_table() (weighted
// prediction — common enough on P slices that skipping it, as an earlier
// version of this parser did, silently dropped detail for most P slices in
// some real streams), and dec_ref_pic_marking(). Returns std::nullopt only
// for syntax this parser doesn't cover: separate_colour_plane (rare 4:4:4
// mode), pic_order_cnt_type 1, and multiple slice groups — all uncommon in
// practice, and safer to decline than to silently misparse.
struct H264SliceHeaderDetail {
    uint32_t firstMbInSlice = 0;
    uint32_t sliceTypeRaw = 0; // 0-9; %5 gives the P/B/I/SP/SI category
    uint32_t picParameterSetId = 0;
    uint32_t frameNum = 0;
    bool hasFieldPicFlag = false; // present only when the SPS isn't frame_mbs_only
    bool fieldPicFlag = false;
    bool bottomFieldFlag = false;

    bool hasIdrPicId = false;
    uint32_t idrPicId = 0;

    bool hasPicOrderCntLsb = false;
    uint32_t picOrderCntLsb = 0;
    bool hasDeltaPicOrderCntBottom = false;
    int32_t deltaPicOrderCntBottom = 0;

    bool hasRedundantPicCnt = false;
    uint32_t redundantPicCnt = 0;

    bool hasDirectSpatialMvPredFlag = false;
    bool directSpatialMvPredFlag = false;

    bool hasNumRefIdxActiveOverrideFlag = false;
    bool numRefIdxActiveOverrideFlag = false;
    uint32_t numRefIdxL0ActiveMinus1 = 0;
    uint32_t numRefIdxL1ActiveMinus1 = 0;

    bool hasDecRefPicMarking = false;
    bool noOutputOfPriorPicsFlag = false;       // IDR only
    bool longTermReferenceFlag = false;         // IDR only
    bool adaptiveRefPicMarkingModeFlag = false; // non-IDR reference pictures only

    bool hasCabacInitIdc = false;
    uint32_t cabacInitIdc = 0;

    int32_t sliceQpDelta = 0;

    bool hasSliceQsDelta = false; // SP/SI only
    bool spForSwitchFlag = false;
    int32_t sliceQsDelta = 0;

    bool hasDeblockingFields = false;
    uint32_t disableDeblockingFilterIdc = 0;
    int32_t sliceAlphaC0OffsetDiv2 = 0;
    int32_t sliceBetaOffsetDiv2 = 0;
};

[[nodiscard]] std::optional<H264SliceHeaderDetail> parseH264SliceHeaderDetail(
    const uint8_t* rbsp, std::size_t size, uint8_t naluType, uint8_t nalRefIdc,
    const H264Sps& sps, const H264Pps& pps);

// Full H.265 slice_segment_header() breakdown, mirroring
// parseH264SliceHeaderDetail()'s role. Needs sps/pps.hasSliceHeaderContext
// (see H265Sps/H265Pps) — returns std::nullopt without it. Beyond that,
// declines (returns std::nullopt) for: non-first slice segments (same
// reasoning as parseH265SliceType), separate_colour_plane, an
// inter-predicted short-term RPS supplied directly in the slice header,
// ref_pic_lists_modification() (needs NumPicTotalCurr, which needs
// per-entry used-by-curr-pic flags this parser doesn't track), and
// pred_weight_table() (weighted prediction) — all uncommon in practice.
struct H265SliceHeaderDetail {
    bool firstSliceSegmentInPicFlag = false;
    bool hasNoOutputOfPriorPicsFlag = false;
    bool noOutputOfPriorPicsFlag = false;
    uint32_t picParameterSetId = 0;
    uint32_t sliceTypeRaw = 0; // H.265 Table 7-7: 0=B, 1=P, 2=I
    bool hasPicOutputFlag = false;
    bool picOutputFlag = false;

    bool hasPicOrderCntLsb = false; // absent for IDR slices
    uint32_t picOrderCntLsb = 0;
    bool shortTermRefPicSetSpsFlag = false;

    bool hasSliceTemporalMvpEnabledFlag = false;
    bool sliceTemporalMvpEnabledFlag = false;

    bool hasSaoFlags = false;
    bool sliceSaoLumaFlag = false;
    bool sliceSaoChromaFlag = false;

    bool hasNumRefIdxActiveOverrideFlag = false; // P/B only
    bool numRefIdxActiveOverrideFlag = false;
    uint32_t numRefIdxL0ActiveMinus1 = 0;
    uint32_t numRefIdxL1ActiveMinus1 = 0;
    bool hasMvdL1ZeroFlag = false;
    bool mvdL1ZeroFlag = false;
    bool hasCabacInitFlag = false;
    bool cabacInitFlag = false;
    bool hasCollocatedRefIdx = false;
    uint32_t collocatedRefIdx = 0;
    bool hasFiveMinusMaxNumMergeCand = false;
    uint32_t fiveMinusMaxNumMergeCand = 0;

    int32_t sliceQpDelta = 0;
    bool hasSliceChromaQpOffsets = false;
    int32_t sliceCbQpOffset = 0;
    int32_t sliceCrQpOffset = 0;

    bool hasDeblockingFilterOverrideFlag = false;
    bool deblockingFilterOverrideFlag = false;
    bool sliceDeblockingFilterDisabledFlag = false; // valid whenever deblocking is in play at all
    bool hasDeblockingOffsets = false;
    int32_t sliceBetaOffsetDiv2 = 0;
    int32_t sliceTcOffsetDiv2 = 0;

    bool hasLoopFilterAcrossSlicesFlag = false;
    bool sliceLoopFilterAcrossSlicesEnabledFlag = false;
};

[[nodiscard]] std::optional<H265SliceHeaderDetail> parseH265SliceHeaderDetail(
    const uint8_t* rbsp, std::size_t size, uint8_t naluType, const H265Sps& sps,
    const H265Pps& pps);

} // namespace bitxray

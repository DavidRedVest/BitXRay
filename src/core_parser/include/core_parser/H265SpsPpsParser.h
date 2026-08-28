#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace bitxray {

struct H265ProfileTierLevel {
    uint8_t generalProfileSpace = 0;
    bool generalTierFlag = false;
    uint8_t generalProfileIdc = 0;
    uint8_t generalLevelIdc = 0;
};

struct H265Sps {
    uint8_t videoParameterSetId = 0;
    uint8_t maxSubLayersMinus1 = 0;
    H265ProfileTierLevel ptl;
    uint32_t seqParameterSetId = 0;
    uint32_t chromaFormatIdc = 1;
    uint32_t bitDepthLumaMinus8 = 0;
    uint32_t bitDepthChromaMinus8 = 0;
    uint32_t log2MaxPicOrderCntLsbMinus4 = 0;

    // Final display dimensions, conformance-window cropping already applied.
    uint32_t width = 0;
    uint32_t height = 0;

    // Everything below is only valid if hasSliceHeaderContext is true.
    // Populated by a best-effort continuation of parsing past what the
    // fields above need, purely to support the slice_segment_header()
    // detail view — failure here (an inter-predicted reference picture
    // set, which this doesn't implement) never invalidates the fields
    // above. See parseH265Sps()'s doc comment.
    bool hasSliceHeaderContext = false;
    bool separateColourPlaneFlag = false;
    bool spsTemporalMvpEnabledFlag = false;
    bool sampleAdaptiveOffsetEnabledFlag = false;
    uint32_t numShortTermRefPicSets = 0;
    bool longTermRefPicsPresentFlag = false;
    uint32_t numLongTermRefPicsSps = 0;
};

struct H265Pps {
    uint32_t picParameterSetId = 0;
    uint32_t seqParameterSetId = 0;
    bool dependentSliceSegmentsEnabledFlag = false;
    bool outputFlagPresentFlag = false;
    uint8_t numExtraSliceHeaderBits = 0;
    bool signDataHidingEnabledFlag = false;
    bool cabacInitPresentFlag = false;
    uint32_t numRefIdxL0DefaultActiveMinus1 = 0;
    uint32_t numRefIdxL1DefaultActiveMinus1 = 0;
    int32_t initQpMinus26 = 0;

    // Everything below is only valid if hasSliceHeaderContext is true; see
    // H265Sps::hasSliceHeaderContext.
    bool hasSliceHeaderContext = false;
    bool ppsSliceChromaQpOffsetsPresentFlag = false;
    bool weightedPredFlag = false;
    bool weightedBipredFlag = false;
    bool tilesEnabledFlag = false;
    bool entropyCodingSyncEnabledFlag = false;
    bool ppsLoopFilterAcrossSlicesEnabledFlag = false;
    bool deblockingFilterOverrideEnabledFlag = false;
    bool ppsDeblockingFilterDisabledFlag = false;
    int32_t ppsBetaOffsetDiv2 = 0;
    int32_t ppsTcOffsetDiv2 = 0;
    bool listsModificationPresentFlag = false;
    bool sliceSegmentHeaderExtensionPresentFlag = false;
};

// Parses an SPS RBSP payload with the 2-byte NAL header already stripped and
// emulation-prevention bytes already removed via unescapeRbsp. Always
// parses through the fields needed for the syntax-element display view
// (profile, level, chroma format, bit depth, resolution); best-effort
// continues further for hasSliceHeaderContext fields (see there).
[[nodiscard]] std::optional<H265Sps> parseH265Sps(const uint8_t* rbsp, std::size_t size);

// Parses a PPS RBSP payload under the same preconditions as parseH265Sps.
[[nodiscard]] std::optional<H265Pps> parseH265Pps(const uint8_t* rbsp, std::size_t size);

} // namespace bitxray

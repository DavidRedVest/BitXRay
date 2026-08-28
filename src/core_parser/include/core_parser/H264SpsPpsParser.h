#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace bitxray {

struct H264Sps {
    uint8_t profileIdc = 0;
    uint8_t levelIdc = 0;
    uint32_t seqParameterSetId = 0;
    uint32_t chromaFormatIdc = 1; // defaults to 4:2:0 when the high-profile extension is absent
    uint32_t bitDepthLumaMinus8 = 0;
    uint32_t bitDepthChromaMinus8 = 0;
    uint32_t log2MaxFrameNumMinus4 = 0;
    uint32_t picOrderCntType = 0;
    uint32_t log2MaxPicOrderCntLsbMinus4 = 0; // only meaningful when picOrderCntType == 0
    uint32_t maxNumRefFrames = 0;
    bool frameMbsOnlyFlag = false;
    bool mbAdaptiveFrameFieldFlag = false;
    bool frameCroppingFlag = false;

    // Final display dimensions, cropping already applied.
    uint32_t width = 0;
    uint32_t height = 0;
};

struct H264Pps {
    uint32_t picParameterSetId = 0;
    uint32_t seqParameterSetId = 0;
    bool entropyCodingModeFlag = false; // false = CAVLC, true = CABAC
    bool bottomFieldPicOrderInFramePresentFlag = false;
    uint32_t numSliceGroupsMinus1 = 0;
    uint32_t numRefIdxL0DefaultActiveMinus1 = 0;
    uint32_t numRefIdxL1DefaultActiveMinus1 = 0;
    bool weightedPredFlag = false;
    uint8_t weightedBipredIdc = 0;
    int32_t picInitQpMinus26 = 0;
    int32_t picInitQsMinus26 = 0;
    int32_t chromaQpIndexOffset = 0;
    bool deblockingFilterControlPresentFlag = false;
    bool constrainedIntraPredFlag = false;
    bool redundantPicCntPresentFlag = false;
};

// Parses an SPS RBSP payload (NALU payload with the 1-byte NAL header already
// stripped and emulation-prevention bytes already removed via unescapeRbsp).
// Returns std::nullopt if the payload is too short/malformed to parse.
[[nodiscard]] std::optional<H264Sps> parseH264Sps(const uint8_t* rbsp, std::size_t size);

// Parses a PPS RBSP payload under the same preconditions as parseH264Sps.
// Note: PPS syntax after redundant_pic_cnt_present_flag (slice groups map
// details, transform_8x8 extension) is intentionally not parsed since it
// isn't needed for the syntax-element display view.
[[nodiscard]] std::optional<H264Pps> parseH264Pps(const uint8_t* rbsp, std::size_t size);

} // namespace bitxray

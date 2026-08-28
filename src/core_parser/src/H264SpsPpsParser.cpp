#include "core_parser/H264SpsPpsParser.h"

#include "core_parser/BitReader.h"
#include "core_parser/ExpGolomb.h"

namespace bitxray {

namespace {

bool isHighProfileWithChromaInfo(uint8_t profileIdc) {
    switch (profileIdc) {
        case 100: case 110: case 122: case 244: case 44:
        case 83: case 86: case 118: case 128: case 138:
        case 139: case 134: case 135:
            return true;
        default:
            return false;
    }
}

// Consumes (without needing the values) one scaling_list() as defined in
// spec 7.3.2.1.1.1 — needed only to keep the bit cursor correctly positioned
// when seq_scaling_matrix_present_flag is set.
void skipScalingList(BitReader& reader, int size) {
    int32_t lastScale = 8;
    int32_t nextScale = 8;
    for (int j = 0; j < size; ++j) {
        if (nextScale != 0) {
            const int32_t deltaScale = readSe(reader);
            nextScale = (lastScale + deltaScale + 256) % 256;
        }
        lastScale = (nextScale == 0) ? lastScale : nextScale;
    }
}

} // namespace

std::optional<H264Sps> parseH264Sps(const uint8_t* rbsp, std::size_t size) {
    if (size < 4) {
        return std::nullopt;
    }
    try {
        BitReader reader(rbsp, size);
        H264Sps sps;

        sps.profileIdc = static_cast<uint8_t>(reader.u(8));
        reader.u(8); // constraint_set0..5_flag (6 bits) + reserved_zero_2bits
        sps.levelIdc = static_cast<uint8_t>(reader.u(8));
        sps.seqParameterSetId = readUe(reader);

        if (isHighProfileWithChromaInfo(sps.profileIdc)) {
            sps.chromaFormatIdc = readUe(reader);
            if (sps.chromaFormatIdc == 3) {
                reader.u(1); // separate_colour_plane_flag
            }
            sps.bitDepthLumaMinus8 = readUe(reader);
            sps.bitDepthChromaMinus8 = readUe(reader);
            reader.u(1); // qpprime_y_zero_transform_bypass_flag
            const bool scalingMatrixPresent = reader.u(1) != 0;
            if (scalingMatrixPresent) {
                const int count = (sps.chromaFormatIdc != 3) ? 8 : 12;
                for (int i = 0; i < count; ++i) {
                    const bool listPresent = reader.u(1) != 0;
                    if (listPresent) {
                        skipScalingList(reader, i < 6 ? 16 : 64);
                    }
                }
            }
        }

        sps.log2MaxFrameNumMinus4 = readUe(reader);
        sps.picOrderCntType = readUe(reader);
        if (sps.picOrderCntType == 0) {
            sps.log2MaxPicOrderCntLsbMinus4 = readUe(reader);
        } else if (sps.picOrderCntType == 1) {
            reader.u(1); // delta_pic_order_always_zero_flag
            (void)readSe(reader); // offset_for_non_ref_pic
            (void)readSe(reader); // offset_for_top_to_bottom_field
            const uint32_t numRefFramesInCycle = readUe(reader);
            for (uint32_t i = 0; i < numRefFramesInCycle; ++i) {
                (void)readSe(reader); // offset_for_ref_frame[i]
            }
        }

        sps.maxNumRefFrames = readUe(reader);
        reader.u(1); // gaps_in_frame_num_value_allowed_flag

        const uint32_t picWidthInMbsMinus1 = readUe(reader);
        const uint32_t picHeightInMapUnitsMinus1 = readUe(reader);
        sps.frameMbsOnlyFlag = reader.u(1) != 0;
        if (!sps.frameMbsOnlyFlag) {
            sps.mbAdaptiveFrameFieldFlag = reader.u(1) != 0;
        }
        reader.u(1); // direct_8x8_inference_flag

        sps.frameCroppingFlag = reader.u(1) != 0;
        uint32_t cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;
        if (sps.frameCroppingFlag) {
            cropLeft = readUe(reader);
            cropRight = readUe(reader);
            cropTop = readUe(reader);
            cropBottom = readUe(reader);
        }

        const uint32_t widthInSamples = (picWidthInMbsMinus1 + 1) * 16;
        const uint32_t frameHeightInMbs = (2 - (sps.frameMbsOnlyFlag ? 1 : 0)) *
                                           (picHeightInMapUnitsMinus1 + 1);
        const uint32_t heightInSamples = frameHeightInMbs * 16;

        // Sub-sampling factors per Table 6-1 (chroma_format_idc: 0=mono,
        // 1=4:2:0, 2=4:2:2, 3=4:4:4).
        uint32_t subWidthC = 2, subHeightC = 2;
        if (sps.chromaFormatIdc == 0) { subWidthC = 1; subHeightC = 1; }
        else if (sps.chromaFormatIdc == 2) { subWidthC = 2; subHeightC = 1; }
        else if (sps.chromaFormatIdc == 3) { subWidthC = 1; subHeightC = 1; }

        const uint32_t cropUnitX = subWidthC;
        const uint32_t cropUnitY = subHeightC * (2 - (sps.frameMbsOnlyFlag ? 1 : 0));

        sps.width = widthInSamples - cropUnitX * (cropLeft + cropRight);
        sps.height = heightInSamples - cropUnitY * (cropTop + cropBottom);

        return sps;
    } catch (const BitstreamOverrunError&) {
        return std::nullopt;
    }
}

std::optional<H264Pps> parseH264Pps(const uint8_t* rbsp, std::size_t size) {
    if (size < 1) {
        return std::nullopt;
    }
    try {
        BitReader reader(rbsp, size);
        H264Pps pps;

        pps.picParameterSetId = readUe(reader);
        pps.seqParameterSetId = readUe(reader);
        pps.entropyCodingModeFlag = reader.u(1) != 0;
        pps.bottomFieldPicOrderInFramePresentFlag = reader.u(1) != 0;
        pps.numSliceGroupsMinus1 = readUe(reader);
        if (pps.numSliceGroupsMinus1 > 0) {
            // Slice group mapping details aren't needed for the syntax-tree
            // display; stop here rather than fully parsing them.
            return pps;
        }
        pps.numRefIdxL0DefaultActiveMinus1 = readUe(reader);
        pps.numRefIdxL1DefaultActiveMinus1 = readUe(reader);
        pps.weightedPredFlag = reader.u(1) != 0;
        pps.weightedBipredIdc = static_cast<uint8_t>(reader.u(2));
        pps.picInitQpMinus26 = readSe(reader);
        pps.picInitQsMinus26 = readSe(reader);
        pps.chromaQpIndexOffset = readSe(reader);
        pps.deblockingFilterControlPresentFlag = reader.u(1) != 0;
        pps.constrainedIntraPredFlag = reader.u(1) != 0;
        pps.redundantPicCntPresentFlag = reader.u(1) != 0;

        return pps;
    } catch (const BitstreamOverrunError&) {
        return std::nullopt;
    }
}

} // namespace bitxray

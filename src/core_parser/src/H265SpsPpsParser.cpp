#include "core_parser/H265SpsPpsParser.h"

#include <vector>

#include "core_parser/BitReader.h"
#include "core_parser/ExpGolomb.h"

namespace bitxray {

namespace {

// Thrown (and always caught adjacent to where it's thrown, never escaping
// parseH265Sps/parseH265Pps) to abandon the best-effort "slice header
// context" continuation when it hits syntax this parser doesn't implement
// — inter-predicted reference picture sets, mainly, which requires the
// full 7.4.8 derivation to even know how many bits follow. Distinct from
// BitstreamOverrunError so "unsupported" and "malformed/truncated" stay
// separate concepts, even though both are handled the same way here (stop,
// keep whatever was already parsed).
struct UnsupportedSyntax {};

// Reads profile_tier_level() per spec 7.3.3. Only the general_* fields are
// kept; sub-layer profile/level fields are consumed to keep the bit cursor
// correct but discarded (the syntax-tree view only shows the general tier).
H265ProfileTierLevel parseProfileTierLevel(BitReader& reader, bool profilePresentFlag,
                                            int maxNumSubLayersMinus1) {
    H265ProfileTierLevel ptl;

    if (profilePresentFlag) {
        ptl.generalProfileSpace = static_cast<uint8_t>(reader.u(2));
        ptl.generalTierFlag = reader.u(1) != 0;
        ptl.generalProfileIdc = static_cast<uint8_t>(reader.u(5));
        reader.skipBits(32); // general_profile_compatibility_flag[0..31]
        reader.skipBits(4);  // progressive/interlaced/non_packed/frame_only constraint flags
        reader.skipBits(43); // general_reserved_zero_43bits / profile-specific constraint flags
        reader.skipBits(1);  // general_inbld_flag / general_reserved_zero_bit
    }
    ptl.generalLevelIdc = static_cast<uint8_t>(reader.u(8));

    std::vector<bool> subLayerProfilePresent(maxNumSubLayersMinus1);
    std::vector<bool> subLayerLevelPresent(maxNumSubLayersMinus1);
    for (int i = 0; i < maxNumSubLayersMinus1; ++i) {
        subLayerProfilePresent[i] = reader.u(1) != 0;
        subLayerLevelPresent[i] = reader.u(1) != 0;
    }
    if (maxNumSubLayersMinus1 > 0) {
        for (int i = maxNumSubLayersMinus1; i < 8; ++i) {
            reader.skipBits(2); // reserved_zero_2bits
        }
    }
    for (int i = 0; i < maxNumSubLayersMinus1; ++i) {
        if (subLayerProfilePresent[i]) {
            reader.skipBits(2 + 1 + 5 + 32 + 4 + 43 + 1); // sub_layer profile block, 88 bits
        }
        if (subLayerLevelPresent[i]) {
            reader.skipBits(8); // sub_layer_level_idc[i]
        }
    }
    return ptl;
}

// scaling_list_data(), spec 7.3.4. Values aren't needed for display, just
// correct bit-cursor advancement.
void skipScalingListData(BitReader& reader) {
    for (int sizeId = 0; sizeId < 4; ++sizeId) {
        for (int matrixId = 0; matrixId < 6; matrixId += (sizeId == 3) ? 3 : 1) {
            const bool predModeFlag = reader.u(1) != 0;
            if (!predModeFlag) {
                (void)readUe(reader); // scaling_list_pred_matrix_id_delta
            } else {
                const int coefNum = (sizeId == 0) ? 16 : 64;
                if (sizeId > 1) {
                    (void)readSe(reader); // scaling_list_dc_coef_minus8
                }
                for (int i = 0; i < coefNum; ++i) {
                    (void)readSe(reader); // scaling_list_delta_coef
                }
            }
        }
    }
}

// st_ref_pic_set(stRpsIdx) as embedded in the SPS's own list (spec 7.3.7),
// i.e. never the "extra" one a slice header can append (that variant, with
// delta_idx_minus1, is handled separately in SliceHeaderParser.cpp). Returns
// NumDeltaPocs[stRpsIdx]; throws UnsupportedSyntax for the inter-predicted
// case, which would require the full 7.4.8 derivation to size correctly.
uint32_t parseStRefPicSetInSps(BitReader& reader, int stRpsIdx,
                                const std::vector<uint32_t>& numDeltaPocsSoFar) {
    const bool interRefPicSetPredictionFlag = (stRpsIdx != 0) && reader.u(1) != 0;
    if (interRefPicSetPredictionFlag) {
        const uint32_t refNumDeltaPocs = numDeltaPocsSoFar[static_cast<std::size_t>(stRpsIdx - 1)];
        reader.u(1);           // delta_rps_sign
        (void)readUe(reader);  // abs_delta_rps_minus1
        for (uint32_t j = 0; j <= refNumDeltaPocs; ++j) {
            if (reader.u(1) == 0) {  // used_by_curr_pic_flag[j]
                reader.u(1);         // use_delta_flag[j]
            }
        }
        throw UnsupportedSyntax{};
    }
    const uint32_t numNegativePics = readUe(reader);
    const uint32_t numPositivePics = readUe(reader);
    for (uint32_t i = 0; i < numNegativePics; ++i) {
        (void)readUe(reader); // delta_poc_s0_minus1[i]
        reader.u(1);          // used_by_curr_pic_s0_flag[i]
    }
    for (uint32_t i = 0; i < numPositivePics; ++i) {
        (void)readUe(reader); // delta_poc_s1_minus1[i]
        reader.u(1);          // used_by_curr_pic_s1_flag[i]
    }
    return numNegativePics + numPositivePics;
}

// Best-effort continuation past what parseH265Sps() otherwise needs, purely
// to support the slice-header detail view. Never called in a way that can
// invalidate fields already parsed — see H265Sps::hasSliceHeaderContext.
void parseH265SpsExtra(BitReader& reader, H265Sps& sps) {
    const bool subLayerOrderingInfoPresent = reader.u(1) != 0;
    const int start = subLayerOrderingInfoPresent ? 0 : sps.maxSubLayersMinus1;
    for (int i = start; i <= sps.maxSubLayersMinus1; ++i) {
        (void)readUe(reader); // sps_max_dec_pic_buffering_minus1[i]
        (void)readUe(reader); // sps_max_num_reorder_pics[i]
        (void)readUe(reader); // sps_max_latency_increase_plus1[i]
    }
    (void)readUe(reader); // log2_min_luma_coding_block_size_minus3
    (void)readUe(reader); // log2_diff_max_min_luma_coding_block_size
    (void)readUe(reader); // log2_min_luma_transform_block_size_minus2
    (void)readUe(reader); // log2_diff_max_min_luma_transform_block_size
    (void)readUe(reader); // max_transform_hierarchy_depth_inter
    (void)readUe(reader); // max_transform_hierarchy_depth_intra

    if (reader.u(1) != 0) {   // scaling_list_enabled_flag
        if (reader.u(1) != 0) { // sps_scaling_list_data_present_flag
            skipScalingListData(reader);
        }
    }

    reader.u(1); // amp_enabled_flag
    sps.sampleAdaptiveOffsetEnabledFlag = reader.u(1) != 0;

    if (reader.u(1) != 0) { // pcm_enabled_flag
        reader.u(4);           // pcm_sample_bit_depth_luma_minus1
        reader.u(4);           // pcm_sample_bit_depth_chroma_minus1
        (void)readUe(reader);  // log2_min_pcm_luma_coding_block_size_minus3
        (void)readUe(reader);  // log2_diff_max_min_pcm_luma_coding_block_size
        reader.u(1);           // pcm_loop_filter_disabled_flag
    }

    sps.numShortTermRefPicSets = readUe(reader);
    std::vector<uint32_t> numDeltaPocs;
    numDeltaPocs.reserve(sps.numShortTermRefPicSets);
    for (uint32_t i = 0; i < sps.numShortTermRefPicSets; ++i) {
        numDeltaPocs.push_back(parseStRefPicSetInSps(reader, static_cast<int>(i), numDeltaPocs));
    }

    sps.longTermRefPicsPresentFlag = reader.u(1) != 0;
    if (sps.longTermRefPicsPresentFlag) {
        sps.numLongTermRefPicsSps = readUe(reader);
        for (uint32_t i = 0; i < sps.numLongTermRefPicsSps; ++i) {
            reader.u(static_cast<int>(sps.log2MaxPicOrderCntLsbMinus4 + 4)); // lt_ref_pic_poc_lsb_sps[i]
            reader.u(1);                                                     // used_by_curr_pic_lt_sps_flag[i]
        }
    }

    sps.spsTemporalMvpEnabledFlag = reader.u(1) != 0;
    // strong_intra_smoothing_enabled_flag and VUI parameters aren't needed.
    sps.hasSliceHeaderContext = true;
}

// Best-effort continuation past what parseH265Pps() otherwise needs; see
// parseH265SpsExtra()'s doc comment.
void parseH265PpsExtra(BitReader& reader, H265Pps& pps) {
    reader.u(1); // constrained_intra_pred_flag
    reader.u(1); // transform_skip_enabled_flag
    if (reader.u(1) != 0) { // cu_qp_delta_enabled_flag
        (void)readUe(reader); // diff_cu_qp_delta_depth
    }
    (void)readSe(reader); // pps_cb_qp_offset
    (void)readSe(reader); // pps_cr_qp_offset
    pps.ppsSliceChromaQpOffsetsPresentFlag = reader.u(1) != 0;
    pps.weightedPredFlag = reader.u(1) != 0;
    pps.weightedBipredFlag = reader.u(1) != 0;
    reader.u(1); // transquant_bypass_enabled_flag
    pps.tilesEnabledFlag = reader.u(1) != 0;
    pps.entropyCodingSyncEnabledFlag = reader.u(1) != 0;
    if (pps.tilesEnabledFlag) {
        const uint32_t numTileColumnsMinus1 = readUe(reader);
        const uint32_t numTileRowsMinus1 = readUe(reader);
        if (reader.u(1) == 0) { // !uniform_spacing_flag
            for (uint32_t i = 0; i < numTileColumnsMinus1; ++i) {
                (void)readUe(reader); // column_width_minus1[i]
            }
            for (uint32_t i = 0; i < numTileRowsMinus1; ++i) {
                (void)readUe(reader); // row_height_minus1[i]
            }
        }
        reader.u(1); // loop_filter_across_tiles_enabled_flag
    }
    pps.ppsLoopFilterAcrossSlicesEnabledFlag = reader.u(1) != 0;
    if (reader.u(1) != 0) { // deblocking_filter_control_present_flag
        pps.deblockingFilterOverrideEnabledFlag = reader.u(1) != 0;
        pps.ppsDeblockingFilterDisabledFlag = reader.u(1) != 0;
        if (!pps.ppsDeblockingFilterDisabledFlag) {
            pps.ppsBetaOffsetDiv2 = readSe(reader);
            pps.ppsTcOffsetDiv2 = readSe(reader);
        }
    }
    if (reader.u(1) != 0) { // pps_scaling_list_data_present_flag
        skipScalingListData(reader);
    }
    pps.listsModificationPresentFlag = reader.u(1) != 0;
    (void)readUe(reader); // log2_parallel_merge_level_minus2
    pps.sliceSegmentHeaderExtensionPresentFlag = reader.u(1) != 0;
    // pps_extension flags and beyond aren't needed.
    pps.hasSliceHeaderContext = true;
}

} // namespace

std::optional<H265Sps> parseH265Sps(const uint8_t* rbsp, std::size_t size) {
    if (size < 4) {
        return std::nullopt;
    }
    try {
        BitReader reader(rbsp, size);
        H265Sps sps;

        sps.videoParameterSetId = static_cast<uint8_t>(reader.u(4));
        sps.maxSubLayersMinus1 = static_cast<uint8_t>(reader.u(3));
        reader.u(1); // sps_temporal_id_nesting_flag

        sps.ptl = parseProfileTierLevel(reader, /*profilePresentFlag=*/true, sps.maxSubLayersMinus1);

        sps.seqParameterSetId = readUe(reader);
        sps.chromaFormatIdc = readUe(reader);
        if (sps.chromaFormatIdc == 3) {
            sps.separateColourPlaneFlag = reader.u(1) != 0;
        }
        const uint32_t picWidth = readUe(reader);
        const uint32_t picHeight = readUe(reader);

        const bool conformanceWindowFlag = reader.u(1) != 0;
        uint32_t confLeft = 0, confRight = 0, confTop = 0, confBottom = 0;
        if (conformanceWindowFlag) {
            confLeft = readUe(reader);
            confRight = readUe(reader);
            confTop = readUe(reader);
            confBottom = readUe(reader);
        }

        sps.bitDepthLumaMinus8 = readUe(reader);
        sps.bitDepthChromaMinus8 = readUe(reader);
        sps.log2MaxPicOrderCntLsbMinus4 = readUe(reader);

        // Sub-sampling factors, Table 6-1 (0=mono, 1=4:2:0, 2=4:2:2, 3=4:4:4).
        uint32_t subWidthC = 2, subHeightC = 2;
        if (sps.chromaFormatIdc == 0) { subWidthC = 1; subHeightC = 1; }
        else if (sps.chromaFormatIdc == 2) { subWidthC = 2; subHeightC = 1; }
        else if (sps.chromaFormatIdc == 3) { subWidthC = 1; subHeightC = 1; }

        sps.width = picWidth - subWidthC * (confLeft + confRight);
        sps.height = picHeight - subHeightC * (confTop + confBottom);

        try {
            parseH265SpsExtra(reader, sps);
        } catch (const BitstreamOverrunError&) {
        } catch (const UnsupportedSyntax&) {
        }

        return sps;
    } catch (const BitstreamOverrunError&) {
        return std::nullopt;
    }
}

std::optional<H265Pps> parseH265Pps(const uint8_t* rbsp, std::size_t size) {
    if (size < 1) {
        return std::nullopt;
    }
    try {
        BitReader reader(rbsp, size);
        H265Pps pps;

        pps.picParameterSetId = readUe(reader);
        pps.seqParameterSetId = readUe(reader);
        pps.dependentSliceSegmentsEnabledFlag = reader.u(1) != 0;
        pps.outputFlagPresentFlag = reader.u(1) != 0;
        pps.numExtraSliceHeaderBits = static_cast<uint8_t>(reader.u(3));
        pps.signDataHidingEnabledFlag = reader.u(1) != 0;
        pps.cabacInitPresentFlag = reader.u(1) != 0;
        pps.numRefIdxL0DefaultActiveMinus1 = readUe(reader);
        pps.numRefIdxL1DefaultActiveMinus1 = readUe(reader);
        pps.initQpMinus26 = readSe(reader);

        try {
            parseH265PpsExtra(reader, pps);
        } catch (const BitstreamOverrunError&) {
        } catch (const UnsupportedSyntax&) {
        }

        return pps;
    } catch (const BitstreamOverrunError&) {
        return std::nullopt;
    }
}

} // namespace bitxray

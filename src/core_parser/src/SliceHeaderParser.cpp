#include "core_parser/SliceHeaderParser.h"

#include "core_parser/BitReader.h"
#include "core_parser/ExpGolomb.h"
#include "core_parser/NaluTypes.h"

namespace bitxray {

std::optional<SliceType> parseH264SliceType(const uint8_t* rbsp, std::size_t size) {
    if (size < 1) {
        return std::nullopt;
    }
    try {
        BitReader reader(rbsp, size);
        (void)readUe(reader); // first_mb_in_slice
        const uint32_t sliceType = readUe(reader) % 5;
        switch (sliceType) {
            case 0: return SliceType::P;
            case 1: return SliceType::B;
            case 2: return SliceType::I;
            case 3: return SliceType::SP;
            case 4: return SliceType::SI;
            default: return std::nullopt;
        }
    } catch (const BitstreamOverrunError&) {
        return std::nullopt;
    }
}

std::optional<SliceType> parseH265SliceType(const uint8_t* rbsp, std::size_t size,
                                             uint8_t naluType, uint8_t numExtraSliceHeaderBits) {
    if (size < 1) {
        return std::nullopt;
    }
    try {
        BitReader reader(rbsp, size);
        const bool firstSliceSegmentInPicFlag = reader.u(1) != 0;

        const auto type = static_cast<H265NaluType>(naluType);
        const bool isIrap = type == H265NaluType::BlaWLp || type == H265NaluType::BlaWRadl ||
                             type == H265NaluType::BlaNLp || type == H265NaluType::IdrWRadl ||
                             type == H265NaluType::IdrNLp || type == H265NaluType::Cra;
        if (isIrap) {
            reader.u(1); // no_output_of_prior_pics_flag
        }
        (void)readUe(reader); // slice_pic_parameter_set_id

        if (!firstSliceSegmentInPicFlag) {
            // Decoding slice_segment_address needs PicSizeInCtbsY from the
            // SPS, which this viewer doesn't otherwise need; not worth
            // threading through just for row-color hinting.
            return std::nullopt;
        }

        for (uint8_t i = 0; i < numExtraSliceHeaderBits; ++i) {
            reader.u(1); // slice_reserved_flag[i]
        }

        // H.265 Table 7-7: 0 = B, 1 = P, 2 = I.
        switch (readUe(reader)) {
            case 0: return SliceType::B;
            case 1: return SliceType::P;
            case 2: return SliceType::I;
            default: return std::nullopt;
        }
    } catch (const BitstreamOverrunError&) {
        return std::nullopt;
    }
}

namespace {

// ref_pic_list_modification()'s inner loop, spec 7.3.3.1: read
// modification_of_pic_nums_idc, then its one operand (if any), stopping at
// the value-3 end marker. Same shape used for both L0 and L1.
void skipRefPicListModificationLoop(BitReader& reader) {
    uint32_t idc;
    do {
        idc = readUe(reader);
        if (idc == 0 || idc == 1) {
            (void)readUe(reader); // abs_diff_pic_num_minus1
        } else if (idc == 2) {
            (void)readUe(reader); // long_term_pic_num
        }
    } while (idc != 3);
}

// pred_weight_table(), spec 7.3.3.2. We don't need the actual weight/offset
// values for display, just to consume them correctly so later fields
// (dec_ref_pic_marking, slice_qp_delta, deblocking...) land on the right
// bits — this is what let P slices with weighted prediction enabled show a
// full breakdown instead of falling back to the generic one-liner.
void skipPredWeightTable(BitReader& reader, uint32_t chromaFormatIdc,
                          uint32_t numRefIdxL0ActiveMinus1, uint32_t numRefIdxL1ActiveMinus1,
                          bool isB) {
    (void)readUe(reader); // luma_log2_weight_denom
    if (chromaFormatIdc != 0) {
        (void)readUe(reader); // chroma_log2_weight_denom
    }
    auto skipList = [&](uint32_t refIdxActiveMinus1) {
        for (uint32_t i = 0; i <= refIdxActiveMinus1; ++i) {
            if (reader.u(1) != 0) {   // luma_weight_lX_flag
                (void)readSe(reader); // luma_weight_lX[i]
                (void)readSe(reader); // luma_offset_lX[i]
            }
            if (chromaFormatIdc != 0) {
                if (reader.u(1) != 0) { // chroma_weight_lX_flag
                    for (int j = 0; j < 2; ++j) {
                        (void)readSe(reader); // chroma_weight_lX[i][j]
                        (void)readSe(reader); // chroma_offset_lX[i][j]
                    }
                }
            }
        }
    };
    skipList(numRefIdxL0ActiveMinus1);
    if (isB) {
        skipList(numRefIdxL1ActiveMinus1);
    }
}

} // namespace

std::optional<H264SliceHeaderDetail> parseH264SliceHeaderDetail(const uint8_t* rbsp,
                                                                  std::size_t size,
                                                                  uint8_t naluType,
                                                                  uint8_t nalRefIdc,
                                                                  const H264Sps& sps,
                                                                  const H264Pps& pps) {
    if (size < 1) {
        return std::nullopt;
    }
    // Slice group map syntax beyond a single group isn't parsed into H264Pps
    // (see parseH264Pps), and separate_colour_plane_flag/pic_order_cnt_type
    // 1 aren't tracked in H264Sps — all rare enough that declining is safer
    // than guessing.
    if (pps.numSliceGroupsMinus1 > 0 || sps.chromaFormatIdc == 3 || sps.picOrderCntType == 1) {
        return std::nullopt;
    }

    try {
        BitReader reader(rbsp, size);
        H264SliceHeaderDetail d;

        d.firstMbInSlice = readUe(reader);
        d.sliceTypeRaw = readUe(reader);
        d.picParameterSetId = readUe(reader);

        d.frameNum = reader.u(static_cast<int>(sps.log2MaxFrameNumMinus4 + 4));

        if (!sps.frameMbsOnlyFlag) {
            d.hasFieldPicFlag = true;
            d.fieldPicFlag = reader.u(1) != 0;
            if (d.fieldPicFlag) {
                d.bottomFieldFlag = reader.u(1) != 0;
            }
        }

        const bool isIdr = (naluType == 5);
        if (isIdr) {
            d.hasIdrPicId = true;
            d.idrPicId = readUe(reader);
        }

        if (sps.picOrderCntType == 0) {
            d.hasPicOrderCntLsb = true;
            d.picOrderCntLsb = reader.u(static_cast<int>(sps.log2MaxPicOrderCntLsbMinus4 + 4));
            if (pps.bottomFieldPicOrderInFramePresentFlag && !d.fieldPicFlag) {
                d.hasDeltaPicOrderCntBottom = true;
                d.deltaPicOrderCntBottom = readSe(reader);
            }
        }

        if (pps.redundantPicCntPresentFlag) {
            d.hasRedundantPicCnt = true;
            d.redundantPicCnt = readUe(reader);
        }

        const uint32_t sliceTypeMod = d.sliceTypeRaw % 5;
        const bool isP = sliceTypeMod == 0;
        const bool isB = sliceTypeMod == 1;
        const bool isI = sliceTypeMod == 2;
        const bool isSP = sliceTypeMod == 3;
        const bool isSI = sliceTypeMod == 4;

        if (isB) {
            d.hasDirectSpatialMvPredFlag = true;
            d.directSpatialMvPredFlag = reader.u(1) != 0;
        }

        if (isP || isSP || isB) {
            d.hasNumRefIdxActiveOverrideFlag = true;
            d.numRefIdxActiveOverrideFlag = reader.u(1) != 0;
            if (d.numRefIdxActiveOverrideFlag) {
                d.numRefIdxL0ActiveMinus1 = readUe(reader);
                d.numRefIdxL1ActiveMinus1 = isB ? readUe(reader) : 0;
            } else {
                // Falls back to the PPS defaults — this is what the decoder
                // actually uses, so show the effective value either way.
                d.numRefIdxL0ActiveMinus1 = pps.numRefIdxL0DefaultActiveMinus1;
                d.numRefIdxL1ActiveMinus1 = isB ? pps.numRefIdxL1DefaultActiveMinus1 : 0;
            }
        }

        // ref_pic_list_modification(), spec 7.3.3.1.
        if (!isI && !isSI) {
            if (reader.u(1) != 0) { // ref_pic_list_modification_flag_l0
                skipRefPicListModificationLoop(reader);
            }
        }
        if (isB) {
            if (reader.u(1) != 0) { // ref_pic_list_modification_flag_l1
                skipRefPicListModificationLoop(reader);
            }
        }

        const bool needsPredWeightTable = (pps.weightedPredFlag && (isP || isSP)) ||
                                           (pps.weightedBipredIdc == 1 && isB);
        if (needsPredWeightTable) {
            skipPredWeightTable(reader, sps.chromaFormatIdc, d.numRefIdxL0ActiveMinus1,
                                 d.numRefIdxL1ActiveMinus1, isB);
        }

        if (nalRefIdc != 0) {
            d.hasDecRefPicMarking = true;
            if (isIdr) {
                d.noOutputOfPriorPicsFlag = reader.u(1) != 0;
                d.longTermReferenceFlag = reader.u(1) != 0;
            } else {
                d.adaptiveRefPicMarkingModeFlag = reader.u(1) != 0;
                if (d.adaptiveRefPicMarkingModeFlag) {
                    // memory_management_control_operation loop, spec 7.3.3.3.
                    uint32_t op;
                    do {
                        op = readUe(reader);
                        if (op == 1 || op == 3) (void)readUe(reader); // difference_of_pic_nums_minus1
                        if (op == 2) (void)readUe(reader);            // long_term_pic_num
                        if (op == 3 || op == 6) (void)readUe(reader); // long_term_frame_idx
                        if (op == 4) (void)readUe(reader);            // max_long_term_frame_idx_plus1
                    } while (op != 0);
                }
            }
        }

        if (pps.entropyCodingModeFlag && !isI && !isSI) {
            d.hasCabacInitIdc = true;
            d.cabacInitIdc = readUe(reader);
        }

        d.sliceQpDelta = readSe(reader);

        if (isSP || isSI) {
            d.hasSliceQsDelta = true;
            if (isSP) {
                d.spForSwitchFlag = reader.u(1) != 0;
            }
            d.sliceQsDelta = readSe(reader);
        }

        if (pps.deblockingFilterControlPresentFlag) {
            d.hasDeblockingFields = true;
            d.disableDeblockingFilterIdc = readUe(reader);
            if (d.disableDeblockingFilterIdc != 1) {
                d.sliceAlphaC0OffsetDiv2 = readSe(reader);
                d.sliceBetaOffsetDiv2 = readSe(reader);
            }
        }

        return d;
    } catch (const BitstreamOverrunError&) {
        return std::nullopt;
    }
}

namespace {

// Ceil(Log2(n)), the bit width spec 7.3.6.1 uses for a handful of index
// fields (short_term_ref_pic_set_idx, lt_idx_sps). Callers only invoke this
// when n > 1, so the n <= 1 -> 0 case is a defensive default, not a path
// the spec actually exercises.
int ceilLog2(uint32_t n) {
    int bits = 0;
    while ((1u << bits) < n) {
        ++bits;
    }
    return bits;
}

} // namespace

std::optional<H265SliceHeaderDetail> parseH265SliceHeaderDetail(const uint8_t* rbsp,
                                                                  std::size_t size,
                                                                  uint8_t naluType,
                                                                  const H265Sps& sps,
                                                                  const H265Pps& pps) {
    if (size < 1 || !sps.hasSliceHeaderContext || !pps.hasSliceHeaderContext) {
        return std::nullopt;
    }
    if (sps.separateColourPlaneFlag) {
        return std::nullopt;
    }

    try {
        BitReader reader(rbsp, size);
        H265SliceHeaderDetail d;

        d.firstSliceSegmentInPicFlag = reader.u(1) != 0;

        const auto type = static_cast<H265NaluType>(naluType);
        const bool isIrap = naluType >= 16 && naluType <= 23;
        const bool isIdr = type == H265NaluType::IdrWRadl || type == H265NaluType::IdrNLp;
        if (isIrap) {
            d.hasNoOutputOfPriorPicsFlag = true;
            d.noOutputOfPriorPicsFlag = reader.u(1) != 0;
        }
        d.picParameterSetId = readUe(reader);

        if (!d.firstSliceSegmentInPicFlag) {
            // Needs PicSizeInCtbsY (SPS-derived CTB geometry) for
            // slice_segment_address's bit width — same call as
            // parseH265SliceType makes for the same reason.
            return std::nullopt;
        }

        for (uint8_t i = 0; i < pps.numExtraSliceHeaderBits; ++i) {
            reader.u(1); // slice_reserved_flag[i]
        }
        d.sliceTypeRaw = readUe(reader);

        if (pps.outputFlagPresentFlag) {
            d.hasPicOutputFlag = true;
            d.picOutputFlag = reader.u(1) != 0;
        }
        // separate_colour_plane_flag == 1 already declined above.

        if (!isIdr) {
            d.hasPicOrderCntLsb = true;
            d.picOrderCntLsb = reader.u(static_cast<int>(sps.log2MaxPicOrderCntLsbMinus4 + 4));
            d.shortTermRefPicSetSpsFlag = reader.u(1) != 0;

            if (!d.shortTermRefPicSetSpsFlag) {
                // st_ref_pic_set(num_short_term_ref_pic_sets), called from
                // the slice header — delta_idx_minus1-style inter
                // prediction can appear here (spec 7.3.7); declining it
                // needs the referenced RPS's own NumDeltaPocs, which this
                // parser doesn't retain per-index.
                if (reader.u(1) != 0) { // inter_ref_pic_set_prediction_flag
                    return std::nullopt;
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
            } else if (sps.numShortTermRefPicSets > 1) {
                reader.u(ceilLog2(sps.numShortTermRefPicSets)); // short_term_ref_pic_set_idx
            }

            if (sps.longTermRefPicsPresentFlag) {
                uint32_t numLongTermSps = 0;
                if (sps.numLongTermRefPicsSps > 0) {
                    numLongTermSps = readUe(reader);
                }
                const uint32_t numLongTermPics = readUe(reader);
                for (uint32_t i = 0; i < numLongTermSps + numLongTermPics; ++i) {
                    if (i < numLongTermSps) {
                        if (sps.numLongTermRefPicsSps > 1) {
                            reader.u(ceilLog2(sps.numLongTermRefPicsSps)); // lt_idx_sps[i]
                        }
                    } else {
                        reader.u(static_cast<int>(sps.log2MaxPicOrderCntLsbMinus4 + 4)); // poc_lsb_lt[i]
                        reader.u(1); // used_by_curr_pic_lt_flag[i]
                    }
                    if (reader.u(1) != 0) {   // delta_poc_msb_present_flag[i]
                        (void)readUe(reader); // delta_poc_msb_cycle_lt[i]
                    }
                }
            }

            if (sps.spsTemporalMvpEnabledFlag) {
                d.hasSliceTemporalMvpEnabledFlag = true;
                d.sliceTemporalMvpEnabledFlag = reader.u(1) != 0;
            }
        }

        if (sps.sampleAdaptiveOffsetEnabledFlag) {
            d.hasSaoFlags = true;
            d.sliceSaoLumaFlag = reader.u(1) != 0;
            if (sps.chromaFormatIdc != 0) {
                d.sliceSaoChromaFlag = reader.u(1) != 0;
            }
        }

        // H.265 Table 7-7: 0 = B, 1 = P, 2 = I.
        const bool isB = d.sliceTypeRaw == 0;
        const bool isP = d.sliceTypeRaw == 1;
        if (isP || isB) {
            d.hasNumRefIdxActiveOverrideFlag = true;
            d.numRefIdxActiveOverrideFlag = reader.u(1) != 0;
            if (d.numRefIdxActiveOverrideFlag) {
                d.numRefIdxL0ActiveMinus1 = readUe(reader);
                if (isB) {
                    d.numRefIdxL1ActiveMinus1 = readUe(reader);
                }
            } else {
                d.numRefIdxL0ActiveMinus1 = pps.numRefIdxL0DefaultActiveMinus1;
                d.numRefIdxL1ActiveMinus1 = isB ? pps.numRefIdxL1DefaultActiveMinus1 : 0;
            }

            // ref_pic_lists_modification() needs NumPicTotalCurr (count of
            // reference pictures actually marked "used by current picture"
            // across the RPS), which needs per-entry used-by-curr-pic
            // flags this parser discards rather than retains. Can't tell
            // whether it's present without that, so decline rather than
            // guess wrong and corrupt every field after it.
            if (pps.listsModificationPresentFlag) {
                return std::nullopt;
            }

            if (isB) {
                d.hasMvdL1ZeroFlag = true;
                d.mvdL1ZeroFlag = reader.u(1) != 0;
            }
            if (pps.cabacInitPresentFlag) {
                d.hasCabacInitFlag = true;
                d.cabacInitFlag = reader.u(1) != 0;
            }
            if (d.sliceTemporalMvpEnabledFlag) {
                bool collocatedFromL0Flag = true;
                if (isB) {
                    collocatedFromL0Flag = reader.u(1) != 0;
                }
                const uint32_t activeCount =
                    collocatedFromL0Flag ? d.numRefIdxL0ActiveMinus1 : d.numRefIdxL1ActiveMinus1;
                if (activeCount > 0) {
                    d.hasCollocatedRefIdx = true;
                    d.collocatedRefIdx = readUe(reader);
                }
            }

            const bool needsPredWeightTable =
                (pps.weightedPredFlag && isP) || (pps.weightedBipredFlag && isB);
            if (needsPredWeightTable) {
                // H.265's pred_weight_table() differs from H.264's (delta-
                // coded chroma denom, etc.); not implemented — decline.
                return std::nullopt;
            }

            d.hasFiveMinusMaxNumMergeCand = true;
            d.fiveMinusMaxNumMergeCand = readUe(reader);
        }

        d.sliceQpDelta = readSe(reader);
        if (pps.ppsSliceChromaQpOffsetsPresentFlag) {
            d.hasSliceChromaQpOffsets = true;
            d.sliceCbQpOffset = readSe(reader);
            d.sliceCrQpOffset = readSe(reader);
        }

        d.sliceDeblockingFilterDisabledFlag = pps.ppsDeblockingFilterDisabledFlag;
        if (pps.deblockingFilterOverrideEnabledFlag) {
            d.hasDeblockingFilterOverrideFlag = true;
            d.deblockingFilterOverrideFlag = reader.u(1) != 0;
        }
        if (d.hasDeblockingFilterOverrideFlag && d.deblockingFilterOverrideFlag) {
            d.sliceDeblockingFilterDisabledFlag = reader.u(1) != 0;
            if (!d.sliceDeblockingFilterDisabledFlag) {
                d.hasDeblockingOffsets = true;
                d.sliceBetaOffsetDiv2 = readSe(reader);
                d.sliceTcOffsetDiv2 = readSe(reader);
            }
        }

        if (pps.ppsLoopFilterAcrossSlicesEnabledFlag &&
            (d.sliceSaoLumaFlag || d.sliceSaoChromaFlag || !d.sliceDeblockingFilterDisabledFlag)) {
            d.hasLoopFilterAcrossSlicesFlag = true;
            d.sliceLoopFilterAcrossSlicesEnabledFlag = reader.u(1) != 0;
        }

        // Entry-point offsets and slice-segment-header-extension data
        // aren't needed for display; stop here rather than parse further.

        return d;
    } catch (const BitstreamOverrunError&) {
        return std::nullopt;
    }
}

} // namespace bitxray

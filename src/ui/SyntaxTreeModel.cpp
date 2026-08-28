#include "SyntaxTreeModel.h"

namespace bitxray::ui {

namespace {

// H.264 Table 7-6: raw values 0-4 are P/B/I/SP/SI "for this slice"; 5-9 are
// the same categories but mean "all slices in this picture are this type."
QString sliceTypeRawLabel(uint32_t raw) {
    static const char* const kNames[5] = {"P", "B", "I", "SP", "SI"};
    const QString name = QString::fromUtf8(kNames[raw % 5]);
    return raw >= 5 ? QStringLiteral("%1 slice only").arg(name)
                     : QStringLiteral("%1 slice").arg(name);
}

QString boolLabel(bool value) {
    return value ? QStringLiteral("1 [True]") : QStringLiteral("0 [False]");
}

// H.265 Table 7-7: 0 = B, 1 = P, 2 = I.
QString h265SliceTypeLabel(uint32_t raw) {
    switch (raw) {
        case 0: return QStringLiteral("B slice");
        case 1: return QStringLiteral("P slice");
        case 2: return QStringLiteral("I slice");
        default: return QStringLiteral("reserved");
    }
}

} // namespace

SyntaxTreeModel::SyntaxTreeModel(QWidget* parent) : QTreeWidget(parent) {
    setColumnCount(2);
    setHeaderLabels({QStringLiteral("Field"), QStringLiteral("Value")});
}

void SyntaxTreeModel::addField(const QString& name, const QVariant& value) {
    new QTreeWidgetItem(this, {name, value.toString()});
}

QTreeWidgetItem* SyntaxTreeModel::addChild(QTreeWidgetItem* parent, const QString& name,
                                            const QVariant& value) {
    return new QTreeWidgetItem(parent, {name, value.toString()});
}

void SyntaxTreeModel::clear() {
    QTreeWidget::clear();
}

void SyntaxTreeModel::showMessage(const QString& message) {
    clear();
    new QTreeWidgetItem(this, {message, QString()});
}

void SyntaxTreeModel::showH264Sps(const H264Sps& sps) {
    clear();
    addField(QStringLiteral("profile_idc"), sps.profileIdc);
    addField(QStringLiteral("level_idc"), sps.levelIdc);
    addField(QStringLiteral("seq_parameter_set_id"), sps.seqParameterSetId);
    addField(QStringLiteral("chroma_format_idc"), sps.chromaFormatIdc);
    addField(QStringLiteral("bit_depth_luma"), sps.bitDepthLumaMinus8 + 8);
    addField(QStringLiteral("bit_depth_chroma"), sps.bitDepthChromaMinus8 + 8);
    addField(QStringLiteral("pic_order_cnt_type"), sps.picOrderCntType);
    addField(QStringLiteral("max_num_ref_frames"), sps.maxNumRefFrames);
    addField(QStringLiteral("frame_mbs_only_flag"), sps.frameMbsOnlyFlag);
    addField(QStringLiteral("width"), sps.width);
    addField(QStringLiteral("height"), sps.height);
    expandAll();
}

void SyntaxTreeModel::showH264Pps(const H264Pps& pps) {
    clear();
    addField(QStringLiteral("pic_parameter_set_id"), pps.picParameterSetId);
    addField(QStringLiteral("seq_parameter_set_id"), pps.seqParameterSetId);
    addField(QStringLiteral("entropy_coding_mode"),
             pps.entropyCodingModeFlag ? QStringLiteral("CABAC") : QStringLiteral("CAVLC"));
    addField(QStringLiteral("num_slice_groups_minus1"), pps.numSliceGroupsMinus1);
    addField(QStringLiteral("num_ref_idx_l0_default_active_minus1"),
             pps.numRefIdxL0DefaultActiveMinus1);
    addField(QStringLiteral("num_ref_idx_l1_default_active_minus1"),
             pps.numRefIdxL1DefaultActiveMinus1);
    addField(QStringLiteral("weighted_pred_flag"), pps.weightedPredFlag);
    addField(QStringLiteral("weighted_bipred_idc"), pps.weightedBipredIdc);
    addField(QStringLiteral("pic_init_qp"), pps.picInitQpMinus26 + 26);
    addField(QStringLiteral("pic_init_qs"), pps.picInitQsMinus26 + 26);
    addField(QStringLiteral("chroma_qp_index_offset"), pps.chromaQpIndexOffset);
    addField(QStringLiteral("deblocking_filter_control_present"),
             pps.deblockingFilterControlPresentFlag);
    addField(QStringLiteral("constrained_intra_pred_flag"), pps.constrainedIntraPredFlag);
    expandAll();
}

void SyntaxTreeModel::showH265Sps(const H265Sps& sps) {
    clear();
    addField(QStringLiteral("video_parameter_set_id"), sps.videoParameterSetId);
    addField(QStringLiteral("max_sub_layers_minus1"), sps.maxSubLayersMinus1);
    addField(QStringLiteral("general_profile_space"), sps.ptl.generalProfileSpace);
    addField(QStringLiteral("general_tier_flag"), sps.ptl.generalTierFlag);
    addField(QStringLiteral("general_profile_idc"), sps.ptl.generalProfileIdc);
    addField(QStringLiteral("general_level_idc"), sps.ptl.generalLevelIdc);
    addField(QStringLiteral("seq_parameter_set_id"), sps.seqParameterSetId);
    addField(QStringLiteral("chroma_format_idc"), sps.chromaFormatIdc);
    addField(QStringLiteral("bit_depth_luma"), sps.bitDepthLumaMinus8 + 8);
    addField(QStringLiteral("bit_depth_chroma"), sps.bitDepthChromaMinus8 + 8);
    addField(QStringLiteral("width"), sps.width);
    addField(QStringLiteral("height"), sps.height);
    expandAll();
}

void SyntaxTreeModel::showH264SliceHeader(uint8_t forbiddenZeroBit, uint8_t nalRefIdc,
                                           uint8_t naluType, const H264SliceHeaderDetail& d) {
    clear();

    auto* nal = new QTreeWidgetItem(this, {QStringLiteral("NAL"), QString()});
    addChild(nal, QStringLiteral("forbidden_zero_bit"),
             QStringLiteral("%1 (1 bit)").arg(forbiddenZeroBit));
    addChild(nal, QStringLiteral("nal_ref_idc"), QStringLiteral("%1 (2 bits)").arg(nalRefIdc));
    addChild(nal, QStringLiteral("nal_unit_type"),
             QStringLiteral("%1 (%2) (5 bits)")
                 .arg(naluType)
                 .arg(QString::fromUtf8(naluTypeName(Codec::H264, naluType))));

    auto* rbsp =
        addChild(nal, QStringLiteral("slice_layer_without_partitioning_rbsp()"));
    auto* sh = addChild(rbsp, QStringLiteral("slice_header()"));

    addChild(sh, QStringLiteral("first_mb_in_slice"), d.firstMbInSlice);
    addChild(sh, QStringLiteral("slice_type"),
             QStringLiteral("%1 (%2)").arg(d.sliceTypeRaw).arg(sliceTypeRawLabel(d.sliceTypeRaw)));
    addChild(sh, QStringLiteral("pic_parameter_set_id"), d.picParameterSetId);
    addChild(sh, QStringLiteral("frame_num"), d.frameNum);
    if (d.hasFieldPicFlag) {
        addChild(sh, QStringLiteral("field_pic_flag"), boolLabel(d.fieldPicFlag));
        if (d.fieldPicFlag) {
            addChild(sh, QStringLiteral("bottom_field_flag"), boolLabel(d.bottomFieldFlag));
        }
    }
    if (d.hasIdrPicId) {
        addChild(sh, QStringLiteral("idr_pic_id"), d.idrPicId);
    }
    if (d.hasPicOrderCntLsb) {
        addChild(sh, QStringLiteral("pic_order_cnt_lsb"), d.picOrderCntLsb);
        if (d.hasDeltaPicOrderCntBottom) {
            addChild(sh, QStringLiteral("delta_pic_order_cnt_bottom"), d.deltaPicOrderCntBottom);
        }
    }
    if (d.hasRedundantPicCnt) {
        addChild(sh, QStringLiteral("redundant_pic_cnt"), d.redundantPicCnt);
    }
    if (d.hasDirectSpatialMvPredFlag) {
        addChild(sh, QStringLiteral("direct_spatial_mv_pred_flag"),
                 boolLabel(d.directSpatialMvPredFlag));
    }
    if (d.hasNumRefIdxActiveOverrideFlag) {
        addChild(sh, QStringLiteral("num_ref_idx_active_override_flag"),
                 boolLabel(d.numRefIdxActiveOverrideFlag));
        addChild(sh, QStringLiteral("num_ref_idx_l0_active_minus1 (effective)"),
                 d.numRefIdxL0ActiveMinus1);
        if (d.sliceTypeRaw % 5 == 1) { // B
            addChild(sh, QStringLiteral("num_ref_idx_l1_active_minus1 (effective)"),
                     d.numRefIdxL1ActiveMinus1);
        }
    }
    addChild(sh, QStringLiteral("ref_pic_list_modification()"));
    if (d.hasDecRefPicMarking) {
        auto* drpm = addChild(sh, QStringLiteral("dec_ref_pic_marking()"));
        if (d.hasIdrPicId) {
            addChild(drpm, QStringLiteral("no_output_of_prior_pics_flag"),
                     boolLabel(d.noOutputOfPriorPicsFlag));
            addChild(drpm, QStringLiteral("long_term_reference_flag"),
                     boolLabel(d.longTermReferenceFlag));
        } else {
            addChild(drpm, QStringLiteral("adaptive_ref_pic_marking_mode_flag"),
                     boolLabel(d.adaptiveRefPicMarkingModeFlag));
        }
    }
    if (d.hasCabacInitIdc) {
        addChild(sh, QStringLiteral("cabac_init_idc"), d.cabacInitIdc);
    }
    addChild(sh, QStringLiteral("slice_qp_delta"), d.sliceQpDelta);
    if (d.hasSliceQsDelta) {
        if (d.sliceTypeRaw % 5 == 3) { // SP
            addChild(sh, QStringLiteral("sp_for_switch_flag"), boolLabel(d.spForSwitchFlag));
        }
        addChild(sh, QStringLiteral("slice_qs_delta"), d.sliceQsDelta);
    }
    if (d.hasDeblockingFields) {
        addChild(sh, QStringLiteral("disable_deblocking_filter_idc"), d.disableDeblockingFilterIdc);
        if (d.disableDeblockingFilterIdc != 1) {
            addChild(sh, QStringLiteral("slice_alpha_c0_offset_div2"), d.sliceAlphaC0OffsetDiv2);
            addChild(sh, QStringLiteral("slice_beta_offset_div2"), d.sliceBetaOffsetDiv2);
        }
    }

    addChild(rbsp, QStringLiteral("slice_data()"));
    addChild(rbsp, QStringLiteral("rbsp_slice_trailing_bits()"));

    expandAll();
    resizeColumnToContents(0);
}

void SyntaxTreeModel::showH265SliceHeader(uint8_t forbiddenZeroBit, uint8_t nuhLayerId,
                                           uint8_t nuhTemporalIdPlus1, uint8_t naluType,
                                           const H265SliceHeaderDetail& d) {
    clear();

    auto* nal = new QTreeWidgetItem(this, {QStringLiteral("NAL"), QString()});
    addChild(nal, QStringLiteral("forbidden_zero_bit"),
             QStringLiteral("%1 (1 bit)").arg(forbiddenZeroBit));
    addChild(nal, QStringLiteral("nal_unit_type"),
             QStringLiteral("%1 (%2) (6 bits)")
                 .arg(naluType)
                 .arg(QString::fromUtf8(naluTypeName(Codec::H265, naluType))));
    addChild(nal, QStringLiteral("nuh_layer_id"), QStringLiteral("%1 (6 bits)").arg(nuhLayerId));
    addChild(nal, QStringLiteral("nuh_temporal_id_plus1"),
             QStringLiteral("%1 (3 bits)").arg(nuhTemporalIdPlus1));

    auto* rbsp = addChild(nal, QStringLiteral("slice_segment_layer_rbsp()"));
    auto* sh = addChild(rbsp, QStringLiteral("slice_segment_header()"));

    addChild(sh, QStringLiteral("first_slice_segment_in_pic_flag"),
             boolLabel(d.firstSliceSegmentInPicFlag));
    if (d.hasNoOutputOfPriorPicsFlag) {
        addChild(sh, QStringLiteral("no_output_of_prior_pics_flag"),
                 boolLabel(d.noOutputOfPriorPicsFlag));
    }
    addChild(sh, QStringLiteral("slice_pic_parameter_set_id"), d.picParameterSetId);
    addChild(sh, QStringLiteral("slice_type"),
             QStringLiteral("%1 (%2)").arg(d.sliceTypeRaw).arg(h265SliceTypeLabel(d.sliceTypeRaw)));
    if (d.hasPicOutputFlag) {
        addChild(sh, QStringLiteral("pic_output_flag"), boolLabel(d.picOutputFlag));
    }
    if (d.hasPicOrderCntLsb) {
        addChild(sh, QStringLiteral("slice_pic_order_cnt_lsb"), d.picOrderCntLsb);
        addChild(sh, QStringLiteral("short_term_ref_pic_set_sps_flag"),
                 boolLabel(d.shortTermRefPicSetSpsFlag));
    }
    if (d.hasSliceTemporalMvpEnabledFlag) {
        addChild(sh, QStringLiteral("slice_temporal_mvp_enabled_flag"),
                 boolLabel(d.sliceTemporalMvpEnabledFlag));
    }
    if (d.hasSaoFlags) {
        addChild(sh, QStringLiteral("slice_sao_luma_flag"), boolLabel(d.sliceSaoLumaFlag));
        addChild(sh, QStringLiteral("slice_sao_chroma_flag"), boolLabel(d.sliceSaoChromaFlag));
    }
    if (d.hasNumRefIdxActiveOverrideFlag) {
        addChild(sh, QStringLiteral("num_ref_idx_active_override_flag"),
                 boolLabel(d.numRefIdxActiveOverrideFlag));
        addChild(sh, QStringLiteral("num_ref_idx_l0_active_minus1 (effective)"),
                 d.numRefIdxL0ActiveMinus1);
        if (d.sliceTypeRaw == 0) { // B
            addChild(sh, QStringLiteral("num_ref_idx_l1_active_minus1 (effective)"),
                     d.numRefIdxL1ActiveMinus1);
        }
    }
    if (d.hasMvdL1ZeroFlag) {
        addChild(sh, QStringLiteral("mvd_l1_zero_flag"), boolLabel(d.mvdL1ZeroFlag));
    }
    if (d.hasCabacInitFlag) {
        addChild(sh, QStringLiteral("cabac_init_flag"), boolLabel(d.cabacInitFlag));
    }
    if (d.hasCollocatedRefIdx) {
        addChild(sh, QStringLiteral("collocated_ref_idx"), d.collocatedRefIdx);
    }
    if (d.hasFiveMinusMaxNumMergeCand) {
        addChild(sh, QStringLiteral("five_minus_max_num_merge_cand"), d.fiveMinusMaxNumMergeCand);
    }
    addChild(sh, QStringLiteral("slice_qp_delta"), d.sliceQpDelta);
    if (d.hasSliceChromaQpOffsets) {
        addChild(sh, QStringLiteral("slice_cb_qp_offset"), d.sliceCbQpOffset);
        addChild(sh, QStringLiteral("slice_cr_qp_offset"), d.sliceCrQpOffset);
    }
    if (d.hasDeblockingFilterOverrideFlag) {
        addChild(sh, QStringLiteral("deblocking_filter_override_flag"),
                 boolLabel(d.deblockingFilterOverrideFlag));
    }
    addChild(sh, QStringLiteral("slice_deblocking_filter_disabled_flag"),
             boolLabel(d.sliceDeblockingFilterDisabledFlag));
    if (d.hasDeblockingOffsets) {
        addChild(sh, QStringLiteral("slice_beta_offset_div2"), d.sliceBetaOffsetDiv2);
        addChild(sh, QStringLiteral("slice_tc_offset_div2"), d.sliceTcOffsetDiv2);
    }
    if (d.hasLoopFilterAcrossSlicesFlag) {
        addChild(sh, QStringLiteral("slice_loop_filter_across_slices_enabled_flag"),
                 boolLabel(d.sliceLoopFilterAcrossSlicesEnabledFlag));
    }

    addChild(rbsp, QStringLiteral("slice_segment_data()"));
    addChild(rbsp, QStringLiteral("byte_alignment()"));

    expandAll();
    resizeColumnToContents(0);
}

void SyntaxTreeModel::showGenericNalu(const NaluInfo& info, const QString& extraInfo) {
    clear();
    addField(QStringLiteral("codec"),
             info.codec == Codec::H264   ? QStringLiteral("H.264")
             : info.codec == Codec::H265 ? QStringLiteral("H.265")
                                          : QStringLiteral("Unknown"));
    addField(QStringLiteral("nal_unit_type"), info.naluType);
    addField(QStringLiteral("type_name"), QString::fromUtf8(naluTypeName(info.codec, info.naluType)));
    if (!extraInfo.isEmpty()) {
        addField(QStringLiteral("info"), extraInfo.trimmed());
    }
    auto* description = new QTreeWidgetItem(
        this, {QStringLiteral("meaning"), QString::fromUtf8(naluTypeDescription(info.codec, info.naluType))});
    description->setFirstColumnSpanned(false);
    setWordWrap(true);
    expandAll();
    resizeColumnToContents(0);
}

void SyntaxTreeModel::showH265Pps(const H265Pps& pps) {
    clear();
    addField(QStringLiteral("pic_parameter_set_id"), pps.picParameterSetId);
    addField(QStringLiteral("seq_parameter_set_id"), pps.seqParameterSetId);
    addField(QStringLiteral("dependent_slice_segments_enabled_flag"),
             pps.dependentSliceSegmentsEnabledFlag);
    addField(QStringLiteral("output_flag_present_flag"), pps.outputFlagPresentFlag);
    addField(QStringLiteral("num_extra_slice_header_bits"), pps.numExtraSliceHeaderBits);
    addField(QStringLiteral("sign_data_hiding_enabled_flag"), pps.signDataHidingEnabledFlag);
    addField(QStringLiteral("cabac_init_present_flag"), pps.cabacInitPresentFlag);
    addField(QStringLiteral("init_qp"), pps.initQpMinus26 + 26);
    expandAll();
}

} // namespace bitxray::ui

#pragma once

#include <QTreeWidget>

#include "core_parser/H264SpsPpsParser.h"
#include "core_parser/H265SpsPpsParser.h"
#include "core_parser/NaluTypes.h"
#include "core_parser/SliceHeaderParser.h"

namespace bitxray::ui {

// Displays SPS/PPS syntax elements as a tree. Built on QTreeWidget directly
// rather than a custom QAbstractItemModel: the tree's shape (a flat list of
// "field: value" rows under one root per parameter set) is simple enough
// that the extra model/view machinery wouldn't earn its keep.
class SyntaxTreeModel : public QTreeWidget {
    Q_OBJECT

public:
    explicit SyntaxTreeModel(QWidget* parent = nullptr);

    void showH264Sps(const H264Sps& sps);
    void showH264Pps(const H264Pps& pps);
    void showH265Sps(const H265Sps& sps);
    void showH265Pps(const H265Pps& pps);
    // Full slice_header() breakdown for an H.264 slice, matching the depth
    // tools like H264BSAnalyzer show (NAL header bits, then a nested
    // slice_header()/dec_ref_pic_marking() tree).
    void showH264SliceHeader(uint8_t forbiddenZeroBit, uint8_t nalRefIdc, uint8_t naluType,
                              const H264SliceHeaderDetail& detail);
    // H.265 equivalent of showH264SliceHeader().
    void showH265SliceHeader(uint8_t forbiddenZeroBit, uint8_t nuhLayerId,
                              uint8_t nuhTemporalIdPlus1, uint8_t naluType,
                              const H265SliceHeaderDetail& detail);
    // Fallback for any NALU that doesn't have its own parsed syntax tree
    // (slices, SEI, AUD, ...): explains what the NAL type is for and, for
    // slices, includes the already-computed slice-type/GOP-position info.
    void showGenericNalu(const NaluInfo& info, const QString& extraInfo);
    void showMessage(const QString& message);
    void clear();

private:
    void addField(const QString& name, const QVariant& value);
    static QTreeWidgetItem* addChild(QTreeWidgetItem* parent, const QString& name,
                                      const QVariant& value = QVariant());
};

} // namespace bitxray::ui

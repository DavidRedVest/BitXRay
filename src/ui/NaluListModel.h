#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QString>
#include <optional>
#include <vector>

#include "core_parser/H264SpsPpsParser.h"
#include "core_parser/H265SpsPpsParser.h"
#include "core_parser/NaluTypes.h"
#include "core_parser/SliceHeaderParser.h"

namespace bitxray::ui {

// One row of derived, display-ready data alongside the raw NaluInfo from
// Layer 1. Computed once at load time in NaluListModel::load() rather than
// re-parsed on every paint.
struct NaluRow {
    NaluInfo info;
    QString typeName;
    QString extraInfo;
    QColor rowColor;

    // Populated only for parameter-set rows of the matching codec.
    std::optional<H264Sps> h264Sps;
    std::optional<H264Pps> h264Pps;
    std::optional<H265Sps> h265Sps;
    std::optional<H265Pps> h265Pps;

    // NAL header bits, populated for every H.264 NALU (cheap — one byte).
    uint8_t h264ForbiddenZeroBit = 0;
    uint8_t h264NalRefIdc = 0;
    // Same idea, H.265 NAL header (2 bytes): forbidden_zero_bit(1) +
    // nal_unit_type(6) + nuh_layer_id(6) + nuh_temporal_id_plus1(3).
    uint8_t h265ForbiddenZeroBit = 0;
    uint8_t h265NuhLayerId = 0;
    uint8_t h265NuhTemporalIdPlus1 = 0;
    // Full slice_header() breakdown; only populated for H.264 slice rows
    // whose active SPS/PPS were both resolved and whose syntax is within
    // what parseH264SliceHeaderDetail() covers (see its doc comment).
    std::optional<H264SliceHeaderDetail> h264SliceDetail;
    // Same idea, H.265: only populated for slice rows whose active SPS/PPS
    // resolved AND had hasSliceHeaderContext, and whose syntax is within
    // what parseH265SliceHeaderDetail() covers.
    std::optional<H265SliceHeaderDetail> h265SliceDetail;
};

class NaluListModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColumnIndex = 0,
        ColumnOffset,
        ColumnLength,
        ColumnStartCode,
        ColumnNalType,
        ColumnInfo,
        ColumnCount,
    };

    explicit NaluListModel(QObject* parent = nullptr);

    // Parses `data` (owned by the caller — MainWindow keeps the file bytes
    // alive for the lifetime of the loaded document) and rebuilds all rows.
    void load(const uint8_t* data, std::size_t size);
    void clear();

    [[nodiscard]] const NaluInfo* naluAt(int row) const;
    [[nodiscard]] std::vector<NaluInfo> naluInfos() const;
    [[nodiscard]] QString extraInfoAt(int row) const;
    [[nodiscard]] std::optional<H264Sps> h264SpsAt(int row) const;
    [[nodiscard]] std::optional<H264Pps> h264PpsAt(int row) const;
    [[nodiscard]] std::optional<H265Sps> h265SpsAt(int row) const;
    [[nodiscard]] std::optional<H265Pps> h265PpsAt(int row) const;
    [[nodiscard]] std::optional<H264SliceHeaderDetail> h264SliceDetailAt(int row) const;
    [[nodiscard]] uint8_t h264ForbiddenZeroBitAt(int row) const;
    [[nodiscard]] uint8_t h264NalRefIdcAt(int row) const;
    [[nodiscard]] std::optional<H265SliceHeaderDetail> h265SliceDetailAt(int row) const;
    [[nodiscard]] uint8_t h265ForbiddenZeroBitAt(int row) const;
    [[nodiscard]] uint8_t h265NuhLayerIdAt(int row) const;
    [[nodiscard]] uint8_t h265NuhTemporalIdPlus1At(int row) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::vector<NaluRow> rows_;
};

} // namespace bitxray::ui

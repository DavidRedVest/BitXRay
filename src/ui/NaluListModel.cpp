#include "NaluListModel.h"

#include <unordered_map>

#include <QBrush>

#include "core_parser/BitReader.h"
#include "core_parser/ExpGolomb.h"
#include "core_parser/NaluExtractor.h"
#include "core_parser/SliceHeaderParser.h"

namespace bitxray::ui {

namespace {

QColor colorForSliceType(SliceType type) {
    switch (type) {
        case SliceType::I: return QColor(255, 210, 210);  // light red
        case SliceType::P: return QColor(210, 255, 210);  // light green
        case SliceType::B: return QColor(210, 225, 255);  // light blue
        case SliceType::SP: return QColor(255, 240, 200); // light orange
        case SliceType::SI: return QColor(240, 210, 255); // light purple
    }
    return QColor(Qt::white);
}

// SPS/PPS/VPS are all "parameter sets" but distinguishing them by color
// (rather than one shared color) makes it much faster to spot, e.g., a
// stream with an unexpected mid-stream SPS change.
QColor colorForParameterSet(Codec codec, uint8_t naluType) {
    if (codec == Codec::H264) {
        return static_cast<H264NaluType>(naluType) == H264NaluType::Sps
                   ? QColor(255, 255, 190)  // pale yellow: SPS
                   : QColor(255, 224, 178); // pale orange: PPS
    }
    if (codec == Codec::H265) {
        switch (static_cast<H265NaluType>(naluType)) {
            case H265NaluType::Vps: return QColor(220, 210, 255); // pale purple: VPS
            case H265NaluType::Sps: return QColor(255, 255, 190); // pale yellow: SPS
            default: return QColor(255, 224, 178);                // pale orange: PPS
        }
    }
    return QColor(255, 255, 190);
}

QString sliceTypeName(SliceType type) {
    switch (type) {
        case SliceType::I: return QStringLiteral("I");
        case SliceType::P: return QStringLiteral("P");
        case SliceType::B: return QStringLiteral("B");
        case SliceType::SP: return QStringLiteral("SP");
        case SliceType::SI: return QStringLiteral("SI");
    }
    return QStringLiteral("?");
}

// Reads just enough of a slice's RBSP to find which PPS it references,
// without needing to know the PPS/SPS yet (unlike parseH264SliceHeaderDetail,
// which needs both already resolved).
std::optional<uint32_t> peekH264PicParameterSetId(const uint8_t* rbsp, std::size_t size) {
    if (size < 1) {
        return std::nullopt;
    }
    try {
        BitReader reader(rbsp, size);
        (void)readUe(reader); // first_mb_in_slice
        (void)readUe(reader); // slice_type
        return readUe(reader); // pic_parameter_set_id
    } catch (const BitstreamOverrunError&) {
        return std::nullopt;
    }
}

// H.265 equivalent of peekH264PicParameterSetId().
std::optional<uint32_t> peekH265PicParameterSetId(const uint8_t* rbsp, std::size_t size,
                                                   uint8_t naluType) {
    if (size < 1) {
        return std::nullopt;
    }
    try {
        BitReader reader(rbsp, size);
        reader.u(1); // first_slice_segment_in_pic_flag
        if (naluType >= 16 && naluType <= 23) { // IRAP
            reader.u(1); // no_output_of_prior_pics_flag
        }
        return readUe(reader); // slice_pic_parameter_set_id
    } catch (const BitstreamOverrunError&) {
        return std::nullopt;
    }
}

NaluRow buildRow(const NaluInfo& info, const uint8_t* fileData,
                  const std::unordered_map<uint32_t, H264Sps>& h264SpsById,
                  const std::unordered_map<uint32_t, H264Pps>& h264PpsById,
                  const std::unordered_map<uint32_t, H265Sps>& h265SpsById,
                  const std::unordered_map<uint32_t, H265Pps>& h265PpsById) {
    NaluRow row;
    row.info = info;
    row.typeName = QString::fromUtf8(naluTypeName(info.codec, info.naluType));
    row.rowColor = QColor(Qt::white);

    if (info.codec == Codec::H264 && info.length >= 1) {
        const uint8_t headerByte = fileData[info.offset];
        row.h264ForbiddenZeroBit = (headerByte >> 7) & 0x1;
        row.h264NalRefIdc = (headerByte >> 5) & 0x3;
    } else if (info.codec == Codec::H265 && info.length >= 2) {
        const uint8_t byte0 = fileData[info.offset];
        const uint8_t byte1 = fileData[info.offset + 1];
        row.h265ForbiddenZeroBit = (byte0 >> 7) & 0x1;
        row.h265NuhLayerId = static_cast<uint8_t>(((byte0 & 0x1) << 5) | (byte1 >> 3));
        row.h265NuhTemporalIdPlus1 = byte1 & 0x7;
    }

    // NAL header is 1 byte for H.264, 2 bytes for H.265; RBSP parsing (SPS/
    // PPS/slice header) needs the header stripped and emulation-prevention
    // bytes removed first.
    const int headerLen = (info.codec == Codec::H265) ? 2 : 1;
    if (info.length <= static_cast<std::size_t>(headerLen)) {
        row.extraInfo = QStringLiteral("(truncated)");
        return row;
    }
    const auto rbsp = unescapeRbsp(fileData + info.offset + headerLen, info.length - headerLen);

    if (info.isParameterSet()) {
        if (info.codec == Codec::H264) {
            if (static_cast<H264NaluType>(info.naluType) == H264NaluType::Sps) {
                row.h264Sps = parseH264Sps(rbsp.data(), rbsp.size());
                if (row.h264Sps) {
                    row.extraInfo = QStringLiteral("%1x%2, Profile %3, Level %4")
                                         .arg(row.h264Sps->width)
                                         .arg(row.h264Sps->height)
                                         .arg(row.h264Sps->profileIdc)
                                         .arg(row.h264Sps->levelIdc);
                }
            } else if (static_cast<H264NaluType>(info.naluType) == H264NaluType::Pps) {
                row.h264Pps = parseH264Pps(rbsp.data(), rbsp.size());
                if (row.h264Pps) {
                    row.extraInfo = row.h264Pps->entropyCodingModeFlag
                                         ? QStringLiteral("CABAC")
                                         : QStringLiteral("CAVLC");
                }
            }
        } else if (info.codec == Codec::H265) {
            if (static_cast<H265NaluType>(info.naluType) == H265NaluType::Sps) {
                row.h265Sps = parseH265Sps(rbsp.data(), rbsp.size());
                if (row.h265Sps) {
                    row.extraInfo = QStringLiteral("%1x%2, Profile %3")
                                         .arg(row.h265Sps->width)
                                         .arg(row.h265Sps->height)
                                         .arg(row.h265Sps->ptl.generalProfileIdc);
                }
            } else if (static_cast<H265NaluType>(info.naluType) == H265NaluType::Pps) {
                row.h265Pps = parseH265Pps(rbsp.data(), rbsp.size());
            }
        }
        row.rowColor = colorForParameterSet(info.codec, info.naluType);
    } else if (info.isSlice()) {
        std::optional<SliceType> sliceType;
        if (info.codec == Codec::H264) {
            sliceType = parseH264SliceType(rbsp.data(), rbsp.size());
        } else if (info.codec == Codec::H265) {
            sliceType = parseH265SliceType(rbsp.data(), rbsp.size(), info.naluType);
        }
        if (sliceType) {
            row.extraInfo = QStringLiteral("Slice (%1)").arg(sliceTypeName(*sliceType));
            row.rowColor = colorForSliceType(*sliceType);
        } else {
            row.extraInfo = QStringLiteral("Slice");
        }

        // Reuses the same unescaped `rbsp` computed above (slice payloads
        // are often the bulk of the file's bytes — unescaping it a second
        // time here would double that cost across the whole stream) to look
        // up the slice's active SPS/PPS (by ID) and parse the full
        // slice_header() breakdown.
        if (info.codec == Codec::H264) {
            if (const auto ppsId = peekH264PicParameterSetId(rbsp.data(), rbsp.size())) {
                const auto ppsIt = h264PpsById.find(*ppsId);
                if (ppsIt != h264PpsById.end()) {
                    const auto spsIt = h264SpsById.find(ppsIt->second.seqParameterSetId);
                    if (spsIt != h264SpsById.end()) {
                        row.h264SliceDetail = parseH264SliceHeaderDetail(
                            rbsp.data(), rbsp.size(), info.naluType, row.h264NalRefIdc,
                            spsIt->second, ppsIt->second);
                    }
                }
            }
        } else if (info.codec == Codec::H265) {
            if (const auto ppsId =
                    peekH265PicParameterSetId(rbsp.data(), rbsp.size(), info.naluType)) {
                const auto ppsIt = h265PpsById.find(*ppsId);
                if (ppsIt != h265PpsById.end()) {
                    const auto spsIt = h265SpsById.find(ppsIt->second.seqParameterSetId);
                    if (spsIt != h265SpsById.end()) {
                        row.h265SliceDetail = parseH265SliceHeaderDetail(
                            rbsp.data(), rbsp.size(), info.naluType, spsIt->second,
                            ppsIt->second);
                    }
                }
            }
        }
    }

    return row;
}

} // namespace

NaluListModel::NaluListModel(QObject* parent) : QAbstractTableModel(parent) {}

void NaluListModel::load(const uint8_t* data, std::size_t size) {
    beginResetModel();
    rows_.clear();
    const auto nalus = extractNalus(data, size);
    rows_.reserve(nalus.size());

    // Number slices #0, #1, #2... within each GOP (resetting at every
    // keyframe) so the GOP length is visible at a glance in the Info column.
    int gopFrameIndex = -1;

    // Tracks every SPS/PPS seen so far by ID, so a slice referencing one
    // via pic_parameter_set_id can look up its full decoding context for
    // the detailed slice_header() breakdown (parameter sets normally
    // precede the slices that use them).
    std::unordered_map<uint32_t, H264Sps> h264SpsById;
    std::unordered_map<uint32_t, H264Pps> h264PpsById;
    std::unordered_map<uint32_t, H265Sps> h265SpsById;
    std::unordered_map<uint32_t, H265Pps> h265PpsById;

    for (const auto& info : nalus) {
        NaluRow row = buildRow(info, data, h264SpsById, h264PpsById, h265SpsById, h265PpsById);

        if (row.h264Sps) h264SpsById[row.h264Sps->seqParameterSetId] = *row.h264Sps;
        if (row.h264Pps) h264PpsById[row.h264Pps->picParameterSetId] = *row.h264Pps;
        if (row.h265Sps) h265SpsById[row.h265Sps->seqParameterSetId] = *row.h265Sps;
        if (row.h265Pps) h265PpsById[row.h265Pps->picParameterSetId] = *row.h265Pps;

        if (info.isSlice()) {
            gopFrameIndex = info.isKeyframe() ? 0 : gopFrameIndex + 1;
            row.extraInfo += QStringLiteral(" #%1").arg(gopFrameIndex);
        }
        rows_.push_back(std::move(row));
    }
    endResetModel();
}

void NaluListModel::clear() {
    beginResetModel();
    rows_.clear();
    endResetModel();
}

const NaluInfo* NaluListModel::naluAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) {
        return nullptr;
    }
    return &rows_[row].info;
}

std::vector<NaluInfo> NaluListModel::naluInfos() const {
    std::vector<NaluInfo> infos;
    infos.reserve(rows_.size());
    for (const auto& row : rows_) {
        infos.push_back(row.info);
    }
    return infos;
}

QString NaluListModel::extraInfoAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return {};
    return rows_[row].extraInfo;
}

std::optional<H264Sps> NaluListModel::h264SpsAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return std::nullopt;
    return rows_[row].h264Sps;
}

std::optional<H264Pps> NaluListModel::h264PpsAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return std::nullopt;
    return rows_[row].h264Pps;
}

std::optional<H265Sps> NaluListModel::h265SpsAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return std::nullopt;
    return rows_[row].h265Sps;
}

std::optional<H265Pps> NaluListModel::h265PpsAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return std::nullopt;
    return rows_[row].h265Pps;
}

std::optional<H264SliceHeaderDetail> NaluListModel::h264SliceDetailAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return std::nullopt;
    return rows_[row].h264SliceDetail;
}

uint8_t NaluListModel::h264ForbiddenZeroBitAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return 0;
    return rows_[row].h264ForbiddenZeroBit;
}

uint8_t NaluListModel::h264NalRefIdcAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return 0;
    return rows_[row].h264NalRefIdc;
}

std::optional<H265SliceHeaderDetail> NaluListModel::h265SliceDetailAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return std::nullopt;
    return rows_[row].h265SliceDetail;
}

uint8_t NaluListModel::h265ForbiddenZeroBitAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return 0;
    return rows_[row].h265ForbiddenZeroBit;
}

uint8_t NaluListModel::h265NuhLayerIdAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return 0;
    return rows_[row].h265NuhLayerId;
}

uint8_t NaluListModel::h265NuhTemporalIdPlus1At(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) return 0;
    return rows_[row].h265NuhTemporalIdPlus1;
}

int NaluListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

int NaluListModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant NaluListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || static_cast<std::size_t>(index.row()) >= rows_.size()) {
        return {};
    }
    const NaluRow& row = rows_[static_cast<std::size_t>(index.row())];

    if (role == Qt::BackgroundRole) {
        return QBrush(row.rowColor);
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
        case ColumnIndex: return index.row();
        case ColumnOffset: return QStringLiteral("0x%1").arg(row.info.offset, 0, 16);
        case ColumnLength: return static_cast<qulonglong>(row.info.length);
        case ColumnStartCode: return row.info.startCodeLen == 4 ? QStringLiteral("00 00 00 01")
                                                                 : QStringLiteral("00 00 01");
        case ColumnNalType: return row.typeName;
        case ColumnInfo: return row.extraInfo;
        default: return {};
    }
}

QVariant NaluListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
        case ColumnIndex: return QStringLiteral("No.");
        case ColumnOffset: return QStringLiteral("Offset");
        case ColumnLength: return QStringLiteral("Length");
        case ColumnStartCode: return QStringLiteral("Start Code");
        case ColumnNalType: return QStringLiteral("NAL Type");
        case ColumnInfo: return QStringLiteral("Info");
        default: return {};
    }
}

} // namespace bitxray::ui

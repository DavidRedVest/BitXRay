#pragma once

#include <cstddef>
#include <cstdint>

namespace bitxray {

enum class Codec {
    Unknown,
    H264,
    H265,
};

// H.264 (Rec. ITU-T H.264, Table 7-1) nal_unit_type values we care about.
enum class H264NaluType : uint8_t {
    Unspecified = 0,
    SliceNonIdr = 1,
    SliceDataPartitionA = 2,
    SliceDataPartitionB = 3,
    SliceDataPartitionC = 4,
    SliceIdr = 5,
    Sei = 6,
    Sps = 7,
    Pps = 8,
    AccessUnitDelimiter = 9,
    EndOfSequence = 10,
    EndOfStream = 11,
    FillerData = 12,
    SpsExtension = 13,
    Prefix = 14,
    SubsetSps = 15,
    SliceAux = 19,
    SliceExtension = 20,
};

// H.265 (Rec. ITU-T H.265, Table 7-1) nal_unit_type values we care about.
enum class H265NaluType : uint8_t {
    TrailN = 0,
    TrailR = 1,
    TsaN = 2,
    TsaR = 3,
    StsaN = 4,
    StsaR = 5,
    RadlN = 6,
    RadlR = 7,
    RaslN = 8,
    RaslR = 9,
    BlaWLp = 16,
    BlaWRadl = 17,
    BlaNLp = 18,
    IdrWRadl = 19,
    IdrNLp = 20,
    Cra = 21,
    Vps = 32,
    Sps = 33,
    Pps = 34,
    AccessUnitDelimiter = 35,
    Eos = 36,
    Eob = 37,
    FillerData = 38,
    PrefixSei = 39,
    SuffixSei = 40,
};

// One NALU as located by NaluExtractor. `offset`/`length` describe the NALU
// payload (start code excluded) within the source buffer; `offset` is the
// single source of truth that the hex view, syntax tree, and player-sync all
// key off of.
struct NaluInfo {
    std::size_t offset = 0;
    std::size_t length = 0;
    uint8_t startCodeLen = 0;
    Codec codec = Codec::Unknown;
    uint8_t naluType = 0;

    [[nodiscard]] bool isSlice() const;
    [[nodiscard]] bool isParameterSet() const;
    // True for slices that start a new GOP without referencing prior
    // pictures (H.264 IDR; H.265 IRAP: BLA/IDR/CRA). Used both for player
    // seek-restart points and for numbering frames within a GOP in the UI.
    [[nodiscard]] bool isKeyframe() const;
};

[[nodiscard]] const char* naluTypeName(Codec codec, uint8_t naluType);

// One-line human-readable explanation of what a NAL unit type is for,
// shown in the UI when a NALU without its own parsed syntax tree (i.e.
// anything but SPS/PPS) is selected.
[[nodiscard]] const char* naluTypeDescription(Codec codec, uint8_t naluType);

} // namespace bitxray

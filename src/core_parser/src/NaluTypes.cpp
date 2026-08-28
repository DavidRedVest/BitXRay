#include "core_parser/NaluTypes.h"

namespace bitxray {

bool NaluInfo::isSlice() const {
    if (codec == Codec::H264) {
        const auto t = static_cast<H264NaluType>(naluType);
        return t == H264NaluType::SliceNonIdr || t == H264NaluType::SliceIdr ||
               t == H264NaluType::SliceAux || t == H264NaluType::SliceExtension;
    }
    if (codec == Codec::H265) {
        // H.265 VCL NAL unit types are 0-31.
        return naluType <= 31;
    }
    return false;
}

bool NaluInfo::isParameterSet() const {
    if (codec == Codec::H264) {
        const auto t = static_cast<H264NaluType>(naluType);
        return t == H264NaluType::Sps || t == H264NaluType::Pps ||
               t == H264NaluType::SpsExtension || t == H264NaluType::SubsetSps;
    }
    if (codec == Codec::H265) {
        const auto t = static_cast<H265NaluType>(naluType);
        return t == H265NaluType::Vps || t == H265NaluType::Sps || t == H265NaluType::Pps;
    }
    return false;
}

bool NaluInfo::isKeyframe() const {
    if (codec == Codec::H264) {
        return static_cast<H264NaluType>(naluType) == H264NaluType::SliceIdr;
    }
    if (codec == Codec::H265) {
        // IRAP picture types, spec Table 7-1: BLA_W_LP(16) .. RSV_IRAP_VCL23(23).
        return naluType >= 16 && naluType <= 23;
    }
    return false;
}

const char* naluTypeName(Codec codec, uint8_t naluType) {
    if (codec == Codec::H264) {
        switch (static_cast<H264NaluType>(naluType)) {
            case H264NaluType::Unspecified: return "Unspecified";
            case H264NaluType::SliceNonIdr: return "Slice (non-IDR)";
            case H264NaluType::SliceDataPartitionA: return "Slice Data Partition A";
            case H264NaluType::SliceDataPartitionB: return "Slice Data Partition B";
            case H264NaluType::SliceDataPartitionC: return "Slice Data Partition C";
            case H264NaluType::SliceIdr: return "Slice (IDR)";
            case H264NaluType::Sei: return "SEI";
            case H264NaluType::Sps: return "SPS";
            case H264NaluType::Pps: return "PPS";
            case H264NaluType::AccessUnitDelimiter: return "Access Unit Delimiter";
            case H264NaluType::EndOfSequence: return "End of Sequence";
            case H264NaluType::EndOfStream: return "End of Stream";
            case H264NaluType::FillerData: return "Filler Data";
            case H264NaluType::SpsExtension: return "SPS Extension";
            case H264NaluType::Prefix: return "Prefix";
            case H264NaluType::SubsetSps: return "Subset SPS";
            case H264NaluType::SliceAux: return "Slice (Auxiliary)";
            case H264NaluType::SliceExtension: return "Slice Extension";
            default: return "Reserved/Unknown";
        }
    }
    if (codec == Codec::H265) {
        switch (static_cast<H265NaluType>(naluType)) {
            case H265NaluType::TrailN: return "TRAIL_N";
            case H265NaluType::TrailR: return "TRAIL_R";
            case H265NaluType::TsaN: return "TSA_N";
            case H265NaluType::TsaR: return "TSA_R";
            case H265NaluType::StsaN: return "STSA_N";
            case H265NaluType::StsaR: return "STSA_R";
            case H265NaluType::RadlN: return "RADL_N";
            case H265NaluType::RadlR: return "RADL_R";
            case H265NaluType::RaslN: return "RASL_N";
            case H265NaluType::RaslR: return "RASL_R";
            case H265NaluType::BlaWLp: return "BLA_W_LP";
            case H265NaluType::BlaWRadl: return "BLA_W_RADL";
            case H265NaluType::BlaNLp: return "BLA_N_LP";
            case H265NaluType::IdrWRadl: return "IDR_W_RADL";
            case H265NaluType::IdrNLp: return "IDR_N_LP";
            case H265NaluType::Cra: return "CRA";
            case H265NaluType::Vps: return "VPS";
            case H265NaluType::Sps: return "SPS";
            case H265NaluType::Pps: return "PPS";
            case H265NaluType::AccessUnitDelimiter: return "Access Unit Delimiter";
            case H265NaluType::Eos: return "End of Sequence";
            case H265NaluType::Eob: return "End of Bitstream";
            case H265NaluType::FillerData: return "Filler Data";
            case H265NaluType::PrefixSei: return "Prefix SEI";
            case H265NaluType::SuffixSei: return "Suffix SEI";
            default: return "Reserved/Unknown";
        }
    }
    return "Unknown";
}

const char* naluTypeDescription(Codec codec, uint8_t naluType) {
    if (codec == Codec::H264) {
        switch (static_cast<H264NaluType>(naluType)) {
            case H264NaluType::SliceNonIdr:
                return "Coded slice of a non-IDR picture. References previously decoded "
                       "pictures, so it can't be decoded on its own.";
            case H264NaluType::SliceDataPartitionA:
            case H264NaluType::SliceDataPartitionB:
            case H264NaluType::SliceDataPartitionC:
                return "Slice data partition — an alternative, rarely-used encoding where a "
                       "single slice's data is split across multiple NAL units.";
            case H264NaluType::SliceIdr:
                return "Coded slice of an IDR (Instantaneous Decoder Refresh) picture. Starts a "
                       "new GOP: no picture before it is needed to decode it or anything after "
                       "it, which is what makes it a valid random-access/seek point.";
            case H264NaluType::Sei:
                return "Supplemental Enhancement Information. Optional metadata (timing, "
                       "captions, HDR info, ...) that isn't required to decode the picture "
                       "correctly.";
            case H264NaluType::Sps:
                return "Sequence Parameter Set. Coding parameters shared by an entire sequence "
                       "of pictures: resolution, profile/level, chroma format, etc.";
            case H264NaluType::Pps:
                return "Picture Parameter Set. Coding parameters that can vary per picture: "
                       "entropy coding mode (CAVLC/CABAC), slice groups, initial QP, etc.";
            case H264NaluType::AccessUnitDelimiter:
                return "Access Unit Delimiter. Marks the boundary between access units (frames) "
                       "and hints at which slice types the frame contains.";
            case H264NaluType::EndOfSequence:
                return "Marks the end of a coded video sequence.";
            case H264NaluType::EndOfStream:
                return "Marks the end of the entire bitstream.";
            case H264NaluType::FillerData:
                return "Filler data, used only to pad the bitrate. Carries no picture "
                       "information.";
            case H264NaluType::SpsExtension:
                return "Sequence Parameter Set Extension — auxiliary SPS fields (used with "
                       "auxiliary pictures, e.g. alpha planes).";
            case H264NaluType::Prefix:
                return "NAL unit prefix, used with SVC/MVC (scalable/multiview) extensions.";
            case H264NaluType::SubsetSps:
                return "Subset Sequence Parameter Set, used with SVC/MVC extensions.";
            case H264NaluType::SliceAux:
                return "Coded slice of an auxiliary coded picture (e.g. an alpha/transparency "
                       "plane) — not part of the primary decoded picture.";
            case H264NaluType::SliceExtension:
                return "Coded slice extension, used with SVC/MVC extensions.";
            default:
                return "Reserved or unrecognized NAL unit type.";
        }
    }
    if (codec == Codec::H265) {
        switch (static_cast<H265NaluType>(naluType)) {
            case H265NaluType::TrailN:
            case H265NaluType::TrailR:
                return "Trailing picture — a normal picture that follows decoding order "
                       "(TRAIL_R is used as a reference by later pictures, TRAIL_N is not).";
            case H265NaluType::TsaN:
            case H265NaluType::TsaR:
                return "Temporal Sub-layer Access picture — a clean point for switching to a "
                       "higher temporal sub-layer (frame rate).";
            case H265NaluType::StsaN:
            case H265NaluType::StsaR:
                return "Step-wise Temporal Sub-layer Access picture — similar to TSA, allows "
                       "gradual temporal layer switching.";
            case H265NaluType::RadlN:
            case H265NaluType::RadlR:
                return "Random Access Decodable Leading picture — a leading picture that IS "
                       "decodable even when starting from the associated IRAP picture.";
            case H265NaluType::RaslN:
            case H265NaluType::RaslR:
                return "Random Access Skipped Leading picture — a leading picture that is NOT "
                       "decodable when starting from the associated IRAP picture, and must be "
                       "discarded on random access.";
            case H265NaluType::BlaWLp:
            case H265NaluType::BlaWRadl:
            case H265NaluType::BlaNLp:
                return "Broken Link Access picture — an IRAP (GOP-start) picture signaling that "
                       "the encoded stream was spliced and leading pictures may not be "
                       "decodable.";
            case H265NaluType::IdrWRadl:
            case H265NaluType::IdrNLp:
                return "Instantaneous Decoder Refresh picture. Starts a new GOP: no picture "
                       "before it is needed to decode it or anything after it — a valid "
                       "random-access/seek point.";
            case H265NaluType::Cra:
                return "Clean Random Access picture. A GOP-start/seek point like IDR, but may "
                       "have leading pictures (RASL) that depend on an earlier GOP and must be "
                       "discarded when starting playback here.";
            case H265NaluType::Vps:
                return "Video Parameter Set. Coding parameters shared across potentially "
                       "multiple layers/sub-layers of the bitstream (mostly relevant for "
                       "scalable/multiview HEVC).";
            case H265NaluType::Sps:
                return "Sequence Parameter Set. Coding parameters shared by an entire sequence "
                       "of pictures: resolution, profile/level/tier, chroma format, etc.";
            case H265NaluType::Pps:
                return "Picture Parameter Set. Coding parameters that can vary per picture: "
                       "slice header layout, initial QP, tiles, etc.";
            case H265NaluType::AccessUnitDelimiter:
                return "Access Unit Delimiter. Marks the boundary between access units (frames) "
                       "and hints at which slice types the frame contains.";
            case H265NaluType::Eos:
                return "Marks the end of a coded video sequence.";
            case H265NaluType::Eob:
                return "Marks the end of the entire bitstream.";
            case H265NaluType::FillerData:
                return "Filler data, used only to pad the bitrate. Carries no picture "
                       "information.";
            case H265NaluType::PrefixSei:
            case H265NaluType::SuffixSei:
                return "Supplemental Enhancement Information. Optional metadata (timing, "
                       "HDR info, ...) that isn't required to decode the picture correctly.";
            default:
                return "Reserved or unrecognized NAL unit type.";
        }
    }
    return "Unknown codec.";
}

} // namespace bitxray

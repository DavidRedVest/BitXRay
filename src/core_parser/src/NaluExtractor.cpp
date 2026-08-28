#include "core_parser/NaluExtractor.h"

namespace bitxray {

namespace {

struct RawNalu {
    std::size_t offset;      // start of payload, i.e. just past the start code
    std::size_t length;      // payload length, start code and next start code excluded
    uint8_t startCodeLen;    // 3 (00 00 01) or 4 (00 00 00 01)
};

// Locates every Annex-B start code in the buffer and slices the payload
// between consecutive ones. Codec-agnostic: NALU type interpretation happens
// in a later pass.
std::vector<RawNalu> scanStartCodes(const uint8_t* data, std::size_t size) {
    std::vector<RawNalu> raw;
    if (size < 3) {
        return raw;
    }

    std::vector<std::pair<std::size_t, uint8_t>> starts; // (start-code position, code length)
    for (std::size_t i = 0; i + 3 <= size; ++i) {
        if (data[i] != 0x00 || data[i + 1] != 0x00) {
            continue;
        }
        if (data[i + 2] == 0x01) {
            starts.emplace_back(i, 3);
            i += 2; // skip past "00 00 01"; loop's ++i lands us right after it
        } else if (i + 4 <= size && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
            starts.emplace_back(i, 4);
            i += 3;
        }
    }

    raw.reserve(starts.size());
    for (std::size_t idx = 0; idx < starts.size(); ++idx) {
        const auto& [pos, codeLen] = starts[idx];
        const std::size_t payloadStart = pos + codeLen;
        const std::size_t payloadEnd = (idx + 1 < starts.size()) ? starts[idx + 1].first : size;
        if (payloadEnd <= payloadStart) {
            continue; // empty NALU between two adjacent start codes; skip
        }
        raw.push_back(RawNalu{payloadStart, payloadEnd - payloadStart, codeLen});
    }
    return raw;
}

uint8_t h264NaluType(uint8_t headerByte) {
    return headerByte & 0x1F;
}

uint8_t h265NaluType(uint8_t headerByte0) {
    return (headerByte0 >> 1) & 0x3F;
}

} // namespace

Codec detectCodec(const uint8_t* data, std::size_t size) {
    const std::vector<RawNalu> raw = scanStartCodes(data, size);
    for (const RawNalu& nalu : raw) {
        if (nalu.length == 0) {
            continue;
        }
        const uint8_t byte0 = data[nalu.offset];

        // H.265 header's top bit (forbidden_zero_bit) must be 0, same as
        // H.264. Try the H.265 interpretation first since its VPS/SPS/PPS
        // type range (32-34) doesn't collide with any valid H.264 type.
        const uint8_t h265Type = h265NaluType(byte0);
        if (nalu.length >= 2 &&
            (h265Type == static_cast<uint8_t>(H265NaluType::Vps) ||
             h265Type == static_cast<uint8_t>(H265NaluType::Sps) ||
             h265Type == static_cast<uint8_t>(H265NaluType::Pps))) {
            return Codec::H265;
        }

        const uint8_t h264Type = h264NaluType(byte0);
        if (h264Type == static_cast<uint8_t>(H264NaluType::Sps) ||
            h264Type == static_cast<uint8_t>(H264NaluType::Pps)) {
            return Codec::H264;
        }
    }
    return Codec::Unknown;
}

std::vector<NaluInfo> extractNalus(const uint8_t* data, std::size_t size, Codec codec) {
    if (codec == Codec::Unknown) {
        codec = detectCodec(data, size);
    }
    // Default to H.264 if detection still fails (e.g. a stream with only
    // slice NALUs and no parameter sets in view) rather than leaving every
    // NALU untyped.
    if (codec == Codec::Unknown) {
        codec = Codec::H264;
    }

    const std::vector<RawNalu> raw = scanStartCodes(data, size);
    std::vector<NaluInfo> result;
    result.reserve(raw.size());

    for (const RawNalu& nalu : raw) {
        NaluInfo info;
        info.offset = nalu.offset;
        info.length = nalu.length;
        info.startCodeLen = nalu.startCodeLen;
        info.codec = codec;
        info.naluType = (codec == Codec::H265) ? h265NaluType(data[nalu.offset])
                                                : h264NaluType(data[nalu.offset]);
        result.push_back(info);
    }
    return result;
}

} // namespace bitxray

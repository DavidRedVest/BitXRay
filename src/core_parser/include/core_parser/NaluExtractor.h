#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core_parser/NaluTypes.h"

namespace bitxray {

// Scans a raw Annex-B elementary stream buffer for start codes
// (00 00 01 / 00 00 00 01) and returns one NaluInfo per NALU found, with
// exact byte offsets/lengths of the NALU payload (start code excluded).
//
// `codec` selects how naluType is decoded from each NALU's header byte(s):
// H.264 uses a 1-byte header (type in bits 3-7), H.265 uses a 2-byte header
// (type in bits 2-8 of the first two bytes). Pass Codec::Unknown to have the
// codec auto-detected from the first parameter-set NALU encountered.
[[nodiscard]] std::vector<NaluInfo> extractNalus(const uint8_t* data, std::size_t size,
                                                  Codec codec = Codec::Unknown);

// Best-effort codec sniff: looks for the first NALU whose header byte(s)
// decode to a VPS/SPS type under either codec's NAL header layout.
[[nodiscard]] Codec detectCodec(const uint8_t* data, std::size_t size);

} // namespace bitxray

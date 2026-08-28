#pragma once

#include <cstdint>

#include "core_parser/BitReader.h"

namespace bitxray {

// ue(v): unsigned Exp-Golomb decoding, per H.264/H.265 spec 9.1.
[[nodiscard]] uint32_t readUe(BitReader& reader);

// se(v): signed Exp-Golomb decoding, per H.264/H.265 spec 9.1.1.
[[nodiscard]] int32_t readSe(BitReader& reader);

} // namespace bitxray

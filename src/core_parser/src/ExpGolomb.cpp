#include "core_parser/ExpGolomb.h"

namespace bitxray {

uint32_t readUe(BitReader& reader) {
    int leadingZeroBits = 0;
    while (reader.readBit() == 0) {
        ++leadingZeroBits;
        if (leadingZeroBits > 31) {
            // Pathological/corrupt stream guard: ue(v) codes this long don't
            // occur in valid SPS/PPS payloads.
            throw BitstreamOverrunError();
        }
    }
    if (leadingZeroBits == 0) {
        return 0;
    }
    const uint32_t rest = reader.u(leadingZeroBits);
    return (1u << leadingZeroBits) - 1 + rest;
}

int32_t readSe(BitReader& reader) {
    const uint32_t codeNum = readUe(reader);
    const int32_t magnitude = static_cast<int32_t>((codeNum + 1) / 2);
    return (codeNum % 2 == 0) ? -magnitude : magnitude;
}

} // namespace bitxray

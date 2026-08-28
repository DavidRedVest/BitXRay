#include "core_parser/BitReader.h"

namespace bitxray {

std::vector<uint8_t> unescapeRbsp(const uint8_t* data, std::size_t size) {
    std::vector<uint8_t> out;
    out.reserve(size);

    int zeroRun = 0;
    for (std::size_t i = 0; i < size; ++i) {
        const uint8_t byte = data[i];
        if (zeroRun >= 2 && byte == 0x03) {
            // Emulation-prevention byte: drop it, reset the zero run so a
            // legitimate 0x00 0x00 0x03 0x00... sequence is handled correctly.
            zeroRun = 0;
            continue;
        }
        out.push_back(byte);
        zeroRun = (byte == 0x00) ? zeroRun + 1 : 0;
    }
    return out;
}

uint32_t BitReader::readBit() {
    if (bitPos_ >= size_ * 8) {
        throw BitstreamOverrunError();
    }
    const std::size_t byteIndex = bitPos_ / 8;
    const int bitIndex = 7 - static_cast<int>(bitPos_ % 8);
    ++bitPos_;
    return (data_[byteIndex] >> bitIndex) & 0x1;
}

uint32_t BitReader::u(int n) {
    uint32_t value = 0;
    for (int i = 0; i < n; ++i) {
        value = (value << 1) | readBit();
    }
    return value;
}

void BitReader::skipBits(std::size_t n) {
    if (bitPos_ + n > size_ * 8) {
        throw BitstreamOverrunError();
    }
    bitPos_ += n;
}

void BitReader::byteAlign() {
    const std::size_t rem = bitPos_ % 8;
    if (rem != 0) {
        skipBits(8 - rem);
    }
}

} // namespace bitxray

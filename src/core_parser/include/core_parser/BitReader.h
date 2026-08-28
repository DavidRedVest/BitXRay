#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace bitxray {

// Thrown when a read runs past the end of the buffer.
class BitstreamOverrunError : public std::runtime_error {
public:
    BitstreamOverrunError() : std::runtime_error("BitReader: read past end of buffer") {}
};

// Strips H.264/H.265 emulation-prevention bytes (the 0x03 inserted after any
// 0x00 0x00 run so RBSP payload can't contain a start-code) so the result is
// safe to feed straight into BitReader.
[[nodiscard]] std::vector<uint8_t> unescapeRbsp(const uint8_t* data, std::size_t size);

// Reads a byte span bit-by-bit, most-significant-bit first, per the
// H.264/H.265 bitstream convention. Does not own the buffer it reads from.
class BitReader {
public:
    BitReader(const uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    // Reads a single bit (0 or 1).
    uint32_t readBit();

    // Reads `n` bits (n in [0, 32]) as an unsigned integer, MSB first.
    uint32_t u(int n);

    [[nodiscard]] bool byteAligned() const { return bitPos_ % 8 == 0; }
    [[nodiscard]] std::size_t bitPosition() const { return bitPos_; }
    [[nodiscard]] std::size_t bitsRemaining() const { return size_ * 8 - bitPos_; }
    [[nodiscard]] bool hasMoreData() const { return bitPos_ < size_ * 8; }

    void skipBits(std::size_t n);
    void byteAlign();

private:
    const uint8_t* data_;
    std::size_t size_;
    std::size_t bitPos_ = 0;
};

} // namespace bitxray

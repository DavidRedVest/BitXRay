#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace bitxray {

// Layer 2 public API. Deliberately free of any core_parser type: this class
// only accepts raw byte spans from the UI layer (per the three-tier
// decoupling rule) and knows nothing about NaluInfo/offsets. Correlating a
// decoded frame back to the NALU(s) that produced it is the UI layer's job.
enum class VideoCodec {
    H264,
    H265,
};

struct DecodedFrame {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgb24; // packed RGB24, width * height * 3 bytes

    // Echoes whatever `pts` the caller passed to the feedNalu() call that
    // ultimately produced this frame, or -1 if none was set. Decoders
    // reorder pictures (B-frames), so this is the only reliable way for a
    // caller to map a decoded frame back to the input NALU that produced
    // it once frames stop coming out in feed order.
    int64_t pts = -1;
};

// Wraps libavcodec directly (not libavformat) since the input is a raw
// Annex-B elementary stream, not a container: callers feed one NALU
// (including its start code) at a time, which is what allows exact,
// frame-accurate correlation with the NALU list in the UI.
class VideoDecoder {
public:
    explicit VideoDecoder(VideoCodec codec);
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    // Feeds one Annex-B NALU (start code included) to the decoder. `pts`,
    // if non-negative, is attached to the packet and echoed back on
    // whichever DecodedFrame it eventually produces (see DecodedFrame::pts).
    // Returns zero or more frames — H.264/H.265 decoders commonly buffer
    // several pictures before emitting due to B-frame reordering.
    [[nodiscard]] std::vector<DecodedFrame> feedNalu(const uint8_t* data, std::size_t size,
                                                      int64_t pts = -1);

    // Drains any frames buffered inside the decoder at end-of-stream.
    [[nodiscard]] std::vector<DecodedFrame> flush();

    // Same decode as feedNalu()/flush(), but skips the RGB24 conversion
    // entirely and returns just the pts of whatever frame(s) came out.
    // Intended for callers that only need to learn *decode-order-vs-
    // display-order* (e.g. building a display-order index over a whole
    // file) without paying for pixel format conversion they'll throw away.
    [[nodiscard]] std::vector<int64_t> feedNaluPtsOnly(const uint8_t* data, std::size_t size,
                                                        int64_t pts = -1);
    [[nodiscard]] std::vector<int64_t> flushPtsOnly();

    // Same decode as feedNalu(), but only pays the RGB24 conversion cost
    // for the one frame (if any) whose pts equals `neededPts` — every other
    // frame the decoder happens to emit during this call is decoded (can't
    // avoid that) but not converted. For catching up to a specific target
    // several dozen-to-hundred frames into a GOP during a seek, where nine
    // NALUs out of every ten's decoded picture will just be discarded
    // immediately, this is the difference between the seek being
    // perceptible or not — sws_scale is not free.
    [[nodiscard]] std::vector<DecodedFrame> feedNaluForTarget(const uint8_t* data,
                                                               std::size_t size, int64_t pts,
                                                               int64_t neededPts);
    [[nodiscard]] std::vector<DecodedFrame> flushForTarget(int64_t neededPts);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace bitxray

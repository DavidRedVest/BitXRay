#include "core_player/VideoDecoder.h"

#include <cstring>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace bitxray {

struct VideoDecoder::Impl {
    AVCodecContext* codecCtx = nullptr;
    SwsContext* swsCtx = nullptr;
    int swsWidth = 0;
    int swsHeight = 0;
    int swsSrcFormat = -1;

    ~Impl() {
        if (swsCtx) {
            sws_freeContext(swsCtx);
        }
        if (codecCtx) {
            avcodec_free_context(&codecCtx);
        }
    }

    DecodedFrame convertFrameToRgb24(const AVFrame* frame) {
        if (!swsCtx || swsWidth != frame->width || swsHeight != frame->height ||
            swsSrcFormat != frame->format) {
            if (swsCtx) {
                sws_freeContext(swsCtx);
                swsCtx = nullptr;
            }
            swsCtx = sws_getContext(frame->width, frame->height,
                                     static_cast<AVPixelFormat>(frame->format), frame->width,
                                     frame->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr,
                                     nullptr, nullptr);
            swsWidth = frame->width;
            swsHeight = frame->height;
            swsSrcFormat = frame->format;

            if (swsCtx) {
                // Most sources (including both real-world captures used to
                // validate this) don't signal color primaries via VUI, so
                // mirror ffmpeg's own default heuristic (BT.601 for
                // small/SD frames, BT.709 above) and assume studio/limited
                // range input — matching this is what keeps our decode
                // pixel-comparable with `ffmpeg -i ... frame.png`. RGB24
                // output is always full-range.
                const int colorspace = (frame->height > 576) ? SWS_CS_ITU709 : SWS_CS_ITU601;
                const int srcRange = (frame->color_range == AVCOL_RANGE_JPEG) ? 1 : 0;
                sws_setColorspaceDetails(swsCtx, sws_getCoefficients(colorspace), srcRange,
                                          sws_getCoefficients(SWS_CS_ITU709), /*dstRange=*/1, 0,
                                          1 << 16, 1 << 16);
            }
        }
        if (!swsCtx) {
            throw std::runtime_error("VideoDecoder: sws_getContext failed");
        }

        DecodedFrame out;
        out.width = frame->width;
        out.height = frame->height;
        out.pts = frame->pts;
        out.rgb24.resize(static_cast<std::size_t>(frame->width) * frame->height * 3);

        uint8_t* dstData[4] = {out.rgb24.data(), nullptr, nullptr, nullptr};
        int dstLinesize[4] = {frame->width * 3, 0, 0, 0};

        sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, dstData, dstLinesize);
        return out;
    }
};

VideoDecoder::VideoDecoder(VideoCodec codec) : impl_(std::make_unique<Impl>()) {
    const AVCodecID id = (codec == VideoCodec::H264) ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC;
    const AVCodec* decoder = avcodec_find_decoder(id);
    if (!decoder) {
        throw std::runtime_error("VideoDecoder: no FFmpeg decoder available for requested codec");
    }

    impl_->codecCtx = avcodec_alloc_context3(decoder);
    if (!impl_->codecCtx) {
        throw std::runtime_error("VideoDecoder: avcodec_alloc_context3 failed");
    }
    // AVCodecContext defaults to single-threaded (thread_count == 1); 0
    // means "pick automatically from available cores", which is what lets
    // this come anywhere near ffmpeg's own CLI decode speed.
    impl_->codecCtx->thread_count = 0;
    impl_->codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    if (avcodec_open2(impl_->codecCtx, decoder, nullptr) < 0) {
        throw std::runtime_error("VideoDecoder: avcodec_open2 failed");
    }
}

VideoDecoder::~VideoDecoder() = default;

namespace {

// Returns false if the send failed in a way that means no frames should be
// expected from it (not every NALU — SPS/PPS/SEI/AUD-only — yields
// decodable picture data on its own; that's not a fatal error).
bool sendOneNalu(AVCodecContext* codecCtx, const uint8_t* data, std::size_t size, int64_t pts) {
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        throw std::runtime_error("VideoDecoder: av_packet_alloc failed");
    }
    if (av_new_packet(packet, static_cast<int>(size)) < 0) {
        av_packet_free(&packet);
        throw std::runtime_error("VideoDecoder: av_new_packet failed");
    }
    std::memcpy(packet->data, data, size);
    packet->pts = (pts >= 0) ? pts : AV_NOPTS_VALUE;

    const int sendResult = avcodec_send_packet(codecCtx, packet);
    av_packet_free(&packet);
    return sendResult >= 0 || sendResult == AVERROR(EAGAIN);
}

} // namespace

std::vector<DecodedFrame> VideoDecoder::feedNalu(const uint8_t* data, std::size_t size,
                                                  int64_t pts) {
    std::vector<DecodedFrame> frames;
    if (!sendOneNalu(impl_->codecCtx, data, size, pts)) {
        return frames;
    }

    AVFrame* frame = av_frame_alloc();
    while (avcodec_receive_frame(impl_->codecCtx, frame) == 0) {
        frames.push_back(impl_->convertFrameToRgb24(frame));
    }
    av_frame_free(&frame);

    return frames;
}

std::vector<DecodedFrame> VideoDecoder::flush() {
    std::vector<DecodedFrame> frames;
    avcodec_send_packet(impl_->codecCtx, nullptr);

    AVFrame* frame = av_frame_alloc();
    while (avcodec_receive_frame(impl_->codecCtx, frame) == 0) {
        frames.push_back(impl_->convertFrameToRgb24(frame));
    }
    av_frame_free(&frame);

    return frames;
}

std::vector<int64_t> VideoDecoder::feedNaluPtsOnly(const uint8_t* data, std::size_t size,
                                                    int64_t pts) {
    std::vector<int64_t> ptsList;
    if (!sendOneNalu(impl_->codecCtx, data, size, pts)) {
        return ptsList;
    }

    AVFrame* frame = av_frame_alloc();
    while (avcodec_receive_frame(impl_->codecCtx, frame) == 0) {
        ptsList.push_back(frame->pts);
    }
    av_frame_free(&frame);

    return ptsList;
}

std::vector<int64_t> VideoDecoder::flushPtsOnly() {
    std::vector<int64_t> ptsList;
    avcodec_send_packet(impl_->codecCtx, nullptr);

    AVFrame* frame = av_frame_alloc();
    while (avcodec_receive_frame(impl_->codecCtx, frame) == 0) {
        ptsList.push_back(frame->pts);
    }
    av_frame_free(&frame);

    return ptsList;
}

std::vector<DecodedFrame> VideoDecoder::feedNaluForTarget(const uint8_t* data, std::size_t size,
                                                           int64_t pts, int64_t neededPts) {
    std::vector<DecodedFrame> frames;
    if (!sendOneNalu(impl_->codecCtx, data, size, pts)) {
        return frames;
    }

    AVFrame* frame = av_frame_alloc();
    while (avcodec_receive_frame(impl_->codecCtx, frame) == 0) {
        if (frame->pts == neededPts) {
            frames.push_back(impl_->convertFrameToRgb24(frame));
        }
    }
    av_frame_free(&frame);

    return frames;
}

std::vector<DecodedFrame> VideoDecoder::flushForTarget(int64_t neededPts) {
    std::vector<DecodedFrame> frames;
    avcodec_send_packet(impl_->codecCtx, nullptr);

    AVFrame* frame = av_frame_alloc();
    while (avcodec_receive_frame(impl_->codecCtx, frame) == 0) {
        if (frame->pts == neededPts) {
            frames.push_back(impl_->convertFrameToRgb24(frame));
        }
    }
    av_frame_free(&frame);

    return frames;
}

} // namespace bitxray

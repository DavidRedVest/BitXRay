#include "VideoDecodeEngine.h"

#include <exception>

#include <QTimer>

#include "core_player/VideoDecoder.h"

namespace bitxray::ui {

namespace {

QImage toQImage(const DecodedFrame& frame) {
    // QImage(uchar*, ...) shares the buffer by default; .copy() forces a
    // deep copy since `frame` (and its rgb24 vector) won't outlive this call.
    return QImage(frame.rgb24.data(), frame.width, frame.height, frame.width * 3,
                  QImage::Format_RGB888)
        .copy();
}

// How far past the requested NALU we're willing to feed the decoder while
// waiting for its picture to come out of B-frame reorder before giving up.
constexpr int kMaxReorderLookahead = 64;

// Beyond this many NALUs of forward distance, restarting at the target's
// own GOP (bounded cost: ~one GOP's worth of decode + one decoder
// recreation) is assumed cheaper than feeding forward through everything
// in between (cost scales with distance — thousands of frames for a click
// near the end of a long file). Comfortably above a typical GOP size
// (~25-50 in the real files this was tuned against) so crossing a single
// GOP boundary during ordinary sequential playback — where the "next" NALU
// needed is right there regardless — doesn't pay restart overhead for no
// reason.
constexpr int kForwardJumpRestartThreshold = 128;

} // namespace

VideoDecodeEngine::VideoDecodeEngine(QObject* parent)
    : QObject(parent), playbackTimer_(new QTimer(this)) {
    // ~25fps default; raw Annex-B streams don't reliably carry timing info,
    // and this is close enough for both the analysis panel and the
    // standalone player.
    playbackTimer_->setInterval(40);
    connect(playbackTimer_, &QTimer::timeout, this, &VideoDecodeEngine::onTimerTick);
}

VideoDecodeEngine::~VideoDecodeEngine() = default;

bool VideoDecodeEngine::isPlaying() const {
    return playbackTimer_->isActive();
}

void VideoDecodeEngine::clear() {
    playbackTimer_->stop();
    decoder_.reset();
    pendingFrames_.clear();
    lastFedNaluIndex_ = -1;
    currentDisplayPos_ = -1;
    nalus_.clear();
    displayOrder_.clear();
    naluIndexToDisplayPos_.clear();
    fileData_.clear();
    codec_ = Codec::Unknown;
    emit playbackStateChanged(false);
}

void VideoDecodeEngine::loadStream(const std::vector<NaluInfo>& nalus, const QByteArray& fileData) {
    clear();
    if (nalus.empty()) {
        emit streamLoaded(false, QStringLiteral("No NALUs to play"));
        return;
    }

    nalus_ = nalus;
    fileData_ = fileData;
    codec_ = nalus_.front().codec;
    if (codec_ != Codec::H264 && codec_ != Codec::H265) {
        emit streamLoaded(false, QStringLiteral("Unrecognized codec"));
        return;
    }

    try {
        buildDisplayOrderIndex();
    } catch (const std::exception&) {
        // A stream core_parser happily lists NALUs for isn't guaranteed to
        // be something FFmpeg's decoder can actually open/decode (wrong
        // codec guess, corrupt SPS, unsupported profile, ...). Degrade to
        // "no video preview" rather than taking the whole app down — the
        // NALU/hex/syntax-tree views are still fully usable regardless.
        clear();
        emit streamLoaded(false, QStringLiteral("Video preview unavailable for this stream"));
        return;
    }
    if (displayOrder_.empty()) {
        emit streamLoaded(false, QStringLiteral("No decodable frames found"));
        return;
    }

    emit streamLoaded(true, QString());
    goToFrame(0);
}

void VideoDecodeEngine::buildDisplayOrderIndex() {
    // One full sequential decode pass to learn playback order, since that's
    // exactly what libavcodec's own B-frame reordering already computes —
    // parsing POC out of every slice header ourselves would just duplicate
    // it. Pixel data from this pass is thrown away immediately; only the
    // (small) pts ordering is kept.
    const VideoCodec videoCodec = (codec_ == Codec::H264) ? VideoCodec::H264 : VideoCodec::H265;
    VideoDecoder indexer(videoCodec);
    const auto* fileBytes = reinterpret_cast<const uint8_t*>(fileData_.constData());

    auto record = [this](std::vector<int64_t>&& ptsList) {
        for (int64_t pts : ptsList) {
            if (pts < 0) continue;
            naluIndexToDisplayPos_[static_cast<int>(pts)] = static_cast<int>(displayOrder_.size());
            displayOrder_.push_back(static_cast<int>(pts));
        }
    };

    for (std::size_t i = 0; i < nalus_.size(); ++i) {
        const NaluInfo& nalu = nalus_[i];
        const std::size_t start = nalu.offset - nalu.startCodeLen;
        const std::size_t len = nalu.length + nalu.startCodeLen;
        const int64_t pts = nalu.isSlice() ? static_cast<int64_t>(i) : -1;
        record(indexer.feedNaluPtsOnly(fileBytes + start, len, pts));
    }
    record(indexer.flushPtsOnly());
}

int VideoDecodeEngine::findGopStart(int naluIndex) const {
    int gopStart = naluIndex;
    for (int i = naluIndex; i >= 0; --i) {
        if (nalus_[static_cast<std::size_t>(i)].isSlice()) {
            gopStart = i;
            if (nalus_[static_cast<std::size_t>(i)].isKeyframe()) {
                break;
            }
        }
    }
    // Pull in any SPS/PPS/VPS/SEI/AUD NALUs immediately preceding the
    // keyframe slice — decoders need those before they can decode it.
    while (gopStart > 0 && !nalus_[static_cast<std::size_t>(gopStart - 1)].isSlice()) {
        --gopStart;
    }
    return gopStart;
}

void VideoDecodeEngine::restartDecoderFromKeyframeBefore(int naluIndex) {
    const int gopStart = findGopStart(naluIndex);

    const VideoCodec videoCodec = (codec_ == Codec::H264) ? VideoCodec::H264 : VideoCodec::H265;
    decoder_ = std::make_unique<VideoDecoder>(videoCodec);
    pendingFrames_.clear();
    lastFedNaluIndex_ = gopStart - 1;
}

void VideoDecodeEngine::feedOneNalu(int naluIndex) {
    const NaluInfo& nalu = nalus_[static_cast<std::size_t>(naluIndex)];
    const auto* fileBytes = reinterpret_cast<const uint8_t*>(fileData_.constData());
    const std::size_t start = nalu.offset - nalu.startCodeLen;
    const std::size_t len = nalu.length + nalu.startCodeLen;
    const int64_t pts = nalu.isSlice() ? static_cast<int64_t>(naluIndex) : -1;

    for (auto& frame : decoder_->feedNalu(fileBytes + start, len, pts)) {
        if (frame.pts >= 0) {
            pendingFrames_.emplace(frame.pts, toQImage(frame));
        }
    }
    lastFedNaluIndex_ = naluIndex;
}

void VideoDecodeEngine::feedOneNaluForTarget(int naluIndex, int64_t targetPts) {
    const NaluInfo& nalu = nalus_[static_cast<std::size_t>(naluIndex)];
    const auto* fileBytes = reinterpret_cast<const uint8_t*>(fileData_.constData());
    const std::size_t start = nalu.offset - nalu.startCodeLen;
    const std::size_t len = nalu.length + nalu.startCodeLen;
    const int64_t pts = nalu.isSlice() ? static_cast<int64_t>(naluIndex) : -1;

    for (auto& frame : decoder_->feedNaluForTarget(fileBytes + start, len, pts, targetPts)) {
        pendingFrames_.emplace(frame.pts, toQImage(frame));
    }
    lastFedNaluIndex_ = naluIndex;
}

void VideoDecodeEngine::seekToNaluIndex(int targetNaluIndex) {
    if (targetNaluIndex < 0 || static_cast<std::size_t>(targetNaluIndex) >= nalus_.size()) {
        return;
    }
    if (isSeeking_) {
        // Reentrant call: displayImage()'s frameReady emit below is direct
        // (synchronous), and MainWindow's NALU-table sync can call straight
        // back into goToNalu() for the exact frame already mid-display.
        // Safe to ignore — that frame is already being shown.
        return;
    }
    isSeeking_ = true;
    struct SeekGuard {
        bool& flag;
        ~SeekGuard() { flag = false; }
    } seekGuard{isSeeking_};

    // Whether we can reach the target by continuing to feed forward (cheap)
    // or should restart from a keyframe (expensive per-restart, but far
    // cheaper than the alternative) has to be decided in *display* order,
    // not bitstream/NALU order: with B-frames, decode order and display
    // order diverge (e.g. decode order I0 P3 B1 B2, display order I0 B1 B2
    // P3), so a lower NALU index can still be a forward step in playback —
    // comparing raw NALU indices here used to trigger a full GOP restart on
    // almost every B-frame transition during continuous playback, which is
    // what made Play feel like slow manual frame-stepping instead of
    // smooth video.
    const auto targetPosIt = naluIndexToDisplayPos_.find(targetNaluIndex);
    const int targetDisplayPos =
        (targetPosIt != naluIndexToDisplayPos_.end()) ? targetPosIt->second : -1;
    const bool isBackwardOrRepeatSeek = targetDisplayPos >= 0 && targetDisplayPos <= currentDisplayPos_;

    // Separately: even a *forward* request can be far enough ahead of
    // whatever we've already fed into the decoder that "keep feeding
    // forward" would mean synchronously decoding thousands of intervening
    // frames before reaching the target — e.g. clicking a NALU near the
    // end of a long file while the decoder is still sitting near the
    // start. That freezes the UI for the whole time. Restarting directly
    // at the target's own GOP bounds the cost to ~one GOP's worth of
    // decode regardless of how far away the jump is — but only worth the
    // decoder-recreation overhead when the jump is actually large; for an
    // ordinary GOP-boundary crossing during sequential playback, the
    // "next" NALU needed is right there either way.
    const bool isFarForwardJump =
        (targetNaluIndex - lastFedNaluIndex_) > kForwardJumpRestartThreshold;
    const bool targetIsInLaterGop = isFarForwardJump && findGopStart(targetNaluIndex) > lastFedNaluIndex_;

    try {
        bool justRestarted = false;
        if (!decoder_ || isBackwardOrRepeatSeek || targetIsInLaterGop) {
            if (!decoder_ || pendingFrames_.find(targetNaluIndex) == pendingFrames_.end()) {
                restartDecoderFromKeyframeBefore(targetNaluIndex);
                justRestarted = true;
            }
        }

        // Right after a restart we're chasing exactly one target through a
        // GOP we just started decoding from scratch — every frame besides
        // the target will be thrown away, so skip their RGB conversion
        // (feedOneNaluForTarget) rather than paying sws_scale for each one
        // (feedOneNalu, which the smooth-forward-stepping path below still
        // wants, since there every decoded frame is likely to actually be
        // shown soon).
        int fedBeyondTarget = 0;
        while (pendingFrames_.find(targetNaluIndex) == pendingFrames_.end()) {
            if (lastFedNaluIndex_ + 1 >= static_cast<int>(nalus_.size())) {
                auto flushed = justRestarted ? decoder_->flushForTarget(targetNaluIndex)
                                              : decoder_->flush();
                for (auto& frame : flushed) {
                    if (frame.pts >= 0) {
                        pendingFrames_.emplace(frame.pts, toQImage(frame));
                    }
                }
                break;
            }
            if (justRestarted) {
                feedOneNaluForTarget(lastFedNaluIndex_ + 1, targetNaluIndex);
            } else {
                feedOneNalu(lastFedNaluIndex_ + 1);
            }
            if (lastFedNaluIndex_ > targetNaluIndex && ++fedBeyondTarget > kMaxReorderLookahead) {
                break; // shouldn't happen for any sane reorder depth; avoid a runaway loop
            }
        }
    } catch (const std::exception&) {
        // Leave whatever was already displayed in place rather than
        // crashing the whole app over one bad NALU mid-stream.
        return;
    }

    const auto it = pendingFrames_.find(targetNaluIndex);
    if (it == pendingFrames_.end()) {
        return;
    }
    // Copy (cheap: QImage is implicitly shared) rather than pass the
    // map-owned value by reference — it's about to be erased, and
    // displayImage()'s emit happens below regardless.
    const QImage image = it->second;
    // Only the entry we just consumed — NOT "everything with a smaller
    // NALU index" — since reordering means a lower-NALU-index frame (see
    // the comment above) can still be needed for an upcoming display
    // position. The decode-ahead cache is naturally bounded by
    // kMaxReorderLookahead per seek; this cap just guards the pathological
    // case (e.g. many out-of-order seeks in a row without displaying).
    pendingFrames_.erase(it);
    while (pendingFrames_.size() > static_cast<std::size_t>(kMaxReorderLookahead) * 4) {
        pendingFrames_.erase(pendingFrames_.begin());
    }
    displayImage(image, targetNaluIndex);
}

void VideoDecodeEngine::displayImage(const QImage& image, int naluIndex) {
    const auto it = naluIndexToDisplayPos_.find(naluIndex);
    currentDisplayPos_ = (it != naluIndexToDisplayPos_.end()) ? it->second : -1;
    emit frameReady(image, naluIndex, currentDisplayPos_, frameCount());
}

void VideoDecodeEngine::goToFrame(int frameIndex) {
    if (frameIndex < 0 || static_cast<std::size_t>(frameIndex) >= displayOrder_.size()) {
        return;
    }
    seekToNaluIndex(displayOrder_[static_cast<std::size_t>(frameIndex)]);
}

void VideoDecodeEngine::goToNalu(int naluIndex) {
    pause();
    seekToNaluIndex(naluIndex);
}

void VideoDecodeEngine::play() {
    if (displayOrder_.empty() || playbackTimer_->isActive()) {
        return;
    }
    // Restart from the top if we're already at the last frame.
    if (currentDisplayPos_ + 1 >= static_cast<int>(displayOrder_.size())) {
        goToFrame(0);
    }
    playbackTimer_->start();
    emit playbackStateChanged(true);
}

void VideoDecodeEngine::pause() {
    if (!playbackTimer_->isActive()) {
        return;
    }
    playbackTimer_->stop();
    emit playbackStateChanged(false);
}

void VideoDecodeEngine::stop() {
    pause();
    if (!displayOrder_.empty()) {
        goToFrame(0);
    }
}

void VideoDecodeEngine::stepNextFrame() {
    pause();
    goToFrame(currentDisplayPos_ + 1);
}

void VideoDecodeEngine::stepPreviousFrame() {
    pause();
    goToFrame(currentDisplayPos_ - 1);
}

void VideoDecodeEngine::setLoopEnabled(bool enabled) {
    loopEnabled_ = enabled;
}

void VideoDecodeEngine::onTimerTick() {
    if (currentDisplayPos_ + 1 < static_cast<int>(displayOrder_.size())) {
        goToFrame(currentDisplayPos_ + 1);
    } else if (loopEnabled_) {
        goToFrame(0);
    } else {
        pause();
    }
}

} // namespace bitxray::ui

#pragma once

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "core_parser/NaluTypes.h"

class QTimer;

namespace bitxray {
class VideoDecoder;
}

namespace bitxray::ui {

// Layer 3 glue: owns a core_player::VideoDecoder and feeds it NALUs located
// by core_parser's NaluExtractor. This is the only class allowed to know
// about both — VideoDecoder's own public API stays free of core_parser
// types, per the three-tier decoupling rule.
//
// Pulled out of VideoPreviewWidget so the decode/seek/playback-timer logic
// has exactly one implementation shared by both UIs that need it: the
// frame-stepping analysis panel (VideoPreviewWidget) and the standalone
// player window (VideoPlayerWidget) — which want different controls around
// the same underlying engine, not two copies of the engine itself.
//
// Two different orderings matter here and must not be conflated:
//   - "NALU index": position in the bitstream (== position in the NALU
//     table). This is decode order, which for B-frame streams is NOT the
//     same as playback order — encoders emit frames like I0 P3 B1 B2 P6 B4
//     B5 ... so decode position 1 in that example produces the picture that
//     displays *third*.
//   - "display position": position in playback/temporal order, i.e. what
//     "Frame N" means to a human scrubbing through the video.
// loadStream() runs one lightweight decode pass up front (discarding pixel
// data, keeping only the emission order — which libavcodec already
// guarantees is display order) to build the mapping between the two. Actual
// pixel decoding then happens on demand: keeping every decoded frame as a
// QImage simultaneously doesn't scale (the motivating real file here is
// 1280x720 * 5952 frames, ~16GB if fully materialized), so only a small
// decode-ahead cache is kept, and seeking restarts from the nearest
// preceding keyframe when the forward-only decoder can't reach the target.
class VideoDecodeEngine : public QObject {
    Q_OBJECT

public:
    explicit VideoDecodeEngine(QObject* parent = nullptr);
    ~VideoDecodeEngine();

    // Records the NALU layout (offsets into `fileData`), indexes display
    // order, and shows the first frame. Emits streamLoaded(false, ...) and
    // leaves the engine cleared if the stream can't be decoded at all.
    void loadStream(const std::vector<NaluInfo>& nalus, const QByteArray& fileData);
    void clear();

    [[nodiscard]] int frameCount() const { return static_cast<int>(displayOrder_.size()); }
    [[nodiscard]] int currentFrame() const { return currentDisplayPos_; }
    [[nodiscard]] bool isPlaying() const;

public slots:
    // `frameIndex` is a position in display/playback order (0 == first
    // frame shown), matching what frameReady()/currentFrame() use.
    void goToFrame(int frameIndex);
    // Seeks directly to the frame produced by a specific NALU (by its index
    // in the list passed to loadStream) — what the NALU table click-to-seek
    // sync calls.
    void goToNalu(int naluIndex);
    void play();
    void pause();
    // Pauses and rewinds to the first frame.
    void stop();
    void stepNextFrame();
    void stepPreviousFrame();
    // When enabled, reaching the last frame during play() restarts from the
    // first frame instead of stopping.
    void setLoopEnabled(bool enabled);

signals:
    void streamLoaded(bool success, const QString& message);
    // Emitted whenever the displayed frame changes.
    void frameReady(const QImage& image, int naluIndex, int displayPos, int frameCount);
    void playbackStateChanged(bool isPlaying);

private slots:
    void onTimerTick();

private:
    void buildDisplayOrderIndex();
    // Ensures the frame at `targetNaluIndex` (which must be a slice NALU) is
    // in pendingFrames_, decoding forward (or restarting from the nearest
    // keyframe first, if necessary) as needed, then displays it.
    void seekToNaluIndex(int targetNaluIndex);
    // NALU index of the start of the GOP naluIndex belongs to (the nearest
    // preceding keyframe slice, plus any SPS/PPS/SEI/AUD immediately
    // before it). Pure query, no side effects — used both to actually
    // restart the decoder and to decide whether a restart is worthwhile.
    [[nodiscard]] int findGopStart(int naluIndex) const;
    void restartDecoderFromKeyframeBefore(int naluIndex);
    void feedOneNalu(int naluIndex);
    // Same as feedOneNalu(), but only pays the RGB-conversion cost for the
    // frame matching `targetPts` (see VideoDecoder::feedNaluForTarget) —
    // used while catching up to a seek target we just restarted for, where
    // most of the frames decoded along the way will never be shown.
    void feedOneNaluForTarget(int naluIndex, int64_t targetPts);
    void displayImage(const QImage& image, int naluIndex);

    QTimer* playbackTimer_;

    QByteArray fileData_;
    std::vector<NaluInfo> nalus_;
    Codec codec_ = Codec::Unknown;

    // Built once by buildDisplayOrderIndex(): displayOrder_[k] is the NALU
    // index of the frame that plays k-th; naluIndexToDisplayPos_ is its
    // inverse.
    std::vector<int> displayOrder_;
    std::unordered_map<int, int> naluIndexToDisplayPos_;

    std::unique_ptr<VideoDecoder> decoder_;
    int lastFedNaluIndex_ = -1;             // highest index in nalus_ fed to decoder_
    std::map<int64_t, QImage> pendingFrames_; // pts (== nalu index) -> decoded image, not yet shown
    int currentDisplayPos_ = -1;            // position within displayOrder_
    // Guards against reentrancy: displayImage()'s frameReady emit is a
    // direct (synchronous) connection, and its receivers can synchronously
    // call back into goToFrame/goToNalu for the very frame already being
    // displayed (e.g. MainWindow syncing the NALU table selection, which
    // re-triggers a "seek" to the row that's already current). Without this
    // guard that reentrant call and the outer call would both try to erase
    // the same pendingFrames_ entry, the second one via a dangling iterator.
    bool isSeeking_ = false;
    bool loopEnabled_ = false;
};

} // namespace bitxray::ui

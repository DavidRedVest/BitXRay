#pragma once

#include <QByteArray>
#include <QWidget>
#include <vector>

#include "core_parser/NaluTypes.h"

class QLabel;
class QPushButton;
class QImage;

namespace bitxray::ui {

class VideoDecodeEngine;

// Frame-stepping analysis panel embedded in MainWindow: shows the frame
// produced by whichever NALU is selected, with Prev/Play/Next controls for
// stepping through the stream one frame at a time in sync with the NALU
// table. For "just play the whole video like a normal player," see
// VideoPlayerWidget/PlaybackWindow instead — that's a deliberately separate
// UI over the same VideoDecodeEngine, not this panel reused in a bigger box.
class VideoPreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoPreviewWidget(QWidget* parent = nullptr);

    void loadStream(const std::vector<NaluInfo>& nalus, const QByteArray& fileData);

    [[nodiscard]] int frameCount() const;

public slots:
    void goToFrame(int frameIndex);
    void goToNalu(int naluIndex);

signals:
    // Emitted whenever the displayed frame changes, carrying the index (in
    // the list passed to loadStream) of the NALU that produced it.
    void frameChanged(int naluIndex);

private slots:
    void onStreamLoaded(bool success, const QString& message);
    void onFrameReady(const QImage& image, int naluIndex, int displayPos, int frameCount);
    void onPlaybackStateChanged(bool isPlaying);
    void togglePlayback();

private:
    VideoDecodeEngine* engine_;
    QLabel* videoLabel_;
    QPushButton* playPauseButton_;
    QPushButton* prevButton_;
    QPushButton* nextButton_;
    QLabel* frameCounterLabel_;
};

} // namespace bitxray::ui

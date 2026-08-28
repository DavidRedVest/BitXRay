#pragma once

#include <QByteArray>
#include <QImage>
#include <QWidget>
#include <vector>

#include "core_parser/NaluTypes.h"

class QLabel;
class QPushButton;
class QCheckBox;

namespace bitxray::ui {

class VideoDecodeEngine;

// A normal video player modeled on H264BSAnalyzer's Play window: Play/
// Pause, Stop, frame-by-frame step, screenshot-to-file, and loop playback.
// Deliberately does NOT support jumping to an arbitrary position (no seek
// bar) — dragging a seek bar means firing off many arbitrary-position
// decode requests in quick succession, each potentially a GOP-restart
// decode, which is exactly what made an earlier version of this feel like
// it hung. Same reasoning as H264BSAnalyzer's own Play window not having one.
class VideoPlayerWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr);

    void loadStream(const std::vector<NaluInfo>& nalus, const QByteArray& fileData);
    void play();

private slots:
    void onStreamLoaded(bool success, const QString& message);
    void onFrameReady(const QImage& image, int naluIndex, int displayPos, int frameCount);
    void onPlaybackStateChanged(bool isPlaying);
    void togglePlayback();
    void saveScreenshot();

private:
    VideoDecodeEngine* engine_;
    QLabel* videoLabel_;
    QPushButton* prevButton_;
    QPushButton* playPauseButton_;
    QPushButton* nextButton_;
    QPushButton* stopButton_;
    QPushButton* screenshotButton_;
    QCheckBox* loopCheckBox_;
    QLabel* timeLabel_;
    QImage currentImage_; // native-resolution copy of what's on screen, for screenshots
};

} // namespace bitxray::ui

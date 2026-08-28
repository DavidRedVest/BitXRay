#include "VideoPreviewWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "VideoDecodeEngine.h"

namespace bitxray::ui {

VideoPreviewWidget::VideoPreviewWidget(QWidget* parent)
    : QWidget(parent),
      engine_(new VideoDecodeEngine(this)),
      videoLabel_(new QLabel(this)),
      playPauseButton_(new QPushButton(QStringLiteral("Play"), this)),
      prevButton_(new QPushButton(QStringLiteral("|< Prev"), this)),
      nextButton_(new QPushButton(QStringLiteral("Next >|"), this)),
      frameCounterLabel_(new QLabel(QStringLiteral("No stream loaded"), this)) {
    videoLabel_->setAlignment(Qt::AlignCenter);
    videoLabel_->setMinimumSize(160, 120);
    videoLabel_->setStyleSheet(QStringLiteral("background-color: black;"));

    auto* controlsLayout = new QHBoxLayout();
    controlsLayout->addWidget(prevButton_);
    controlsLayout->addWidget(playPauseButton_);
    controlsLayout->addWidget(nextButton_);
    controlsLayout->addStretch();
    controlsLayout->addWidget(frameCounterLabel_);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(videoLabel_, /*stretch=*/1);
    layout->addLayout(controlsLayout);

    playPauseButton_->setEnabled(false);
    prevButton_->setEnabled(false);
    nextButton_->setEnabled(false);

    connect(playPauseButton_, &QPushButton::clicked, this, &VideoPreviewWidget::togglePlayback);
    connect(prevButton_, &QPushButton::clicked, engine_, &VideoDecodeEngine::stepPreviousFrame);
    connect(nextButton_, &QPushButton::clicked, engine_, &VideoDecodeEngine::stepNextFrame);

    connect(engine_, &VideoDecodeEngine::streamLoaded, this, &VideoPreviewWidget::onStreamLoaded);
    connect(engine_, &VideoDecodeEngine::frameReady, this, &VideoPreviewWidget::onFrameReady);
    connect(engine_, &VideoDecodeEngine::playbackStateChanged, this,
            &VideoPreviewWidget::onPlaybackStateChanged);
}

void VideoPreviewWidget::loadStream(const std::vector<NaluInfo>& nalus,
                                    const QByteArray& fileData) {
    engine_->loadStream(nalus, fileData);
}

int VideoPreviewWidget::frameCount() const {
    return engine_->frameCount();
}

void VideoPreviewWidget::goToFrame(int frameIndex) {
    engine_->goToFrame(frameIndex);
}

void VideoPreviewWidget::goToNalu(int naluIndex) {
    engine_->goToNalu(naluIndex);
}

void VideoPreviewWidget::togglePlayback() {
    if (engine_->isPlaying()) {
        engine_->pause();
    } else {
        engine_->play();
    }
}

void VideoPreviewWidget::onStreamLoaded(bool success, const QString& message) {
    playPauseButton_->setEnabled(success);
    prevButton_->setEnabled(success);
    nextButton_->setEnabled(success);
    if (!success) {
        videoLabel_->clear();
        frameCounterLabel_->setText(message.isEmpty() ? QStringLiteral("No stream loaded")
                                                        : message);
    }
}

void VideoPreviewWidget::onFrameReady(const QImage& image, int naluIndex, int displayPos,
                                      int frameCount) {
    videoLabel_->setPixmap(
        QPixmap::fromImage(image).scaled(videoLabel_->size(), Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
    frameCounterLabel_->setText(
        QStringLiteral("Frame %1 / %2").arg(displayPos + 1).arg(frameCount));
    emit frameChanged(naluIndex);
}

void VideoPreviewWidget::onPlaybackStateChanged(bool isPlaying) {
    playPauseButton_->setText(isPlaying ? QStringLiteral("Pause") : QStringLiteral("Play"));
}

} // namespace bitxray::ui

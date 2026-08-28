#include "VideoPlayerWidget.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "VideoDecodeEngine.h"

namespace bitxray::ui {

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent)
    : QWidget(parent),
      engine_(new VideoDecodeEngine(this)),
      videoLabel_(new QLabel(this)),
      prevButton_(new QPushButton(QStringLiteral("|< Prev"), this)),
      playPauseButton_(new QPushButton(QStringLiteral("Play"), this)),
      nextButton_(new QPushButton(QStringLiteral("Next >|"), this)),
      stopButton_(new QPushButton(QStringLiteral("Stop"), this)),
      screenshotButton_(new QPushButton(QStringLiteral("Screenshot..."), this)),
      loopCheckBox_(new QCheckBox(QStringLiteral("Loop"), this)),
      timeLabel_(new QLabel(QStringLiteral("0 / 0"), this)) {
    videoLabel_->setAlignment(Qt::AlignCenter);
    videoLabel_->setMinimumSize(320, 240);
    videoLabel_->setStyleSheet(QStringLiteral("background-color: black;"));

    auto* controlsLayout = new QHBoxLayout();
    controlsLayout->addWidget(prevButton_);
    controlsLayout->addWidget(playPauseButton_);
    controlsLayout->addWidget(nextButton_);
    controlsLayout->addWidget(stopButton_);
    controlsLayout->addWidget(loopCheckBox_);
    controlsLayout->addWidget(screenshotButton_);
    controlsLayout->addStretch();
    controlsLayout->addWidget(timeLabel_);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(videoLabel_, /*stretch=*/1);
    layout->addLayout(controlsLayout);

    prevButton_->setEnabled(false);
    playPauseButton_->setEnabled(false);
    nextButton_->setEnabled(false);
    stopButton_->setEnabled(false);
    screenshotButton_->setEnabled(false);

    connect(prevButton_, &QPushButton::clicked, engine_, &VideoDecodeEngine::stepPreviousFrame);
    connect(playPauseButton_, &QPushButton::clicked, this, &VideoPlayerWidget::togglePlayback);
    connect(nextButton_, &QPushButton::clicked, engine_, &VideoDecodeEngine::stepNextFrame);
    connect(stopButton_, &QPushButton::clicked, engine_, &VideoDecodeEngine::stop);
    connect(loopCheckBox_, &QCheckBox::toggled, engine_, &VideoDecodeEngine::setLoopEnabled);
    connect(screenshotButton_, &QPushButton::clicked, this, &VideoPlayerWidget::saveScreenshot);

    connect(engine_, &VideoDecodeEngine::streamLoaded, this, &VideoPlayerWidget::onStreamLoaded);
    connect(engine_, &VideoDecodeEngine::frameReady, this, &VideoPlayerWidget::onFrameReady);
    connect(engine_, &VideoDecodeEngine::playbackStateChanged, this,
            &VideoPlayerWidget::onPlaybackStateChanged);
}

void VideoPlayerWidget::loadStream(const std::vector<NaluInfo>& nalus,
                                   const QByteArray& fileData) {
    engine_->loadStream(nalus, fileData);
}

void VideoPlayerWidget::play() {
    engine_->play();
}

void VideoPlayerWidget::togglePlayback() {
    if (engine_->isPlaying()) {
        engine_->pause();
    } else {
        engine_->play();
    }
}

void VideoPlayerWidget::saveScreenshot() {
    if (currentImage_.isNull()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Screenshot"), QStringLiteral("frame.png"),
        QStringLiteral("PNG Image (*.png)"));
    if (path.isEmpty()) {
        return;
    }
    if (!currentImage_.save(path)) {
        QMessageBox::warning(this, QStringLiteral("BitXRay"),
                              QStringLiteral("Could not save screenshot to:\n%1").arg(path));
    }
}

void VideoPlayerWidget::onStreamLoaded(bool success, const QString& message) {
    prevButton_->setEnabled(success);
    playPauseButton_->setEnabled(success);
    nextButton_->setEnabled(success);
    stopButton_->setEnabled(success);
    screenshotButton_->setEnabled(success);
    if (!success) {
        videoLabel_->clear();
        currentImage_ = QImage();
        timeLabel_->setText(message.isEmpty() ? QStringLiteral("0 / 0") : message);
    }
}

void VideoPlayerWidget::onFrameReady(const QImage& image, int /*naluIndex*/, int displayPos,
                                     int frameCount) {
    currentImage_ = image;
    videoLabel_->setPixmap(
        QPixmap::fromImage(image).scaled(videoLabel_->size(), Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
    timeLabel_->setText(QStringLiteral("%1 / %2").arg(displayPos + 1).arg(frameCount));
}

void VideoPlayerWidget::onPlaybackStateChanged(bool isPlaying) {
    playPauseButton_->setText(isPlaying ? QStringLiteral("Pause") : QStringLiteral("Play"));
}

} // namespace bitxray::ui

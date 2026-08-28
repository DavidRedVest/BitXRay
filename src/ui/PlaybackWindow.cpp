#include "PlaybackWindow.h"

#include <QVBoxLayout>

#include "VideoPlayerWidget.h"

namespace bitxray::ui {

PlaybackWindow::PlaybackWindow(QWidget* parent)
    : QDialog(parent), player_(new VideoPlayerWidget(this)) {
    setWindowTitle(QStringLiteral("BitXRay — Playback"));
    resize(960, 640);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(player_);
}

void PlaybackWindow::loadStream(const std::vector<NaluInfo>& nalus, const QByteArray& fileData) {
    player_->loadStream(nalus, fileData);
}

void PlaybackWindow::play() {
    player_->play();
}

} // namespace bitxray::ui

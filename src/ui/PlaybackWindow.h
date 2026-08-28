#pragma once

#include <QByteArray>
#include <QDialog>
#include <vector>

#include "core_parser/NaluTypes.h"

namespace bitxray::ui {

class VideoPlayerWidget;

// Standalone "watch the whole thing" window, opened from the toolbar's Play
// action — a normal video player (Play/Pause, Stop, a scrubbable seek bar),
// not the NALU-table-synced frame-stepping panel embedded in MainWindow.
class PlaybackWindow : public QDialog {
    Q_OBJECT

public:
    explicit PlaybackWindow(QWidget* parent = nullptr);

    void loadStream(const std::vector<NaluInfo>& nalus, const QByteArray& fileData);
    void play();

private:
    VideoPlayerWidget* player_;
};

} // namespace bitxray::ui

#pragma once

#include <QByteArray>
#include <QMainWindow>

class QTableView;
class QModelIndex;
class QDragEnterEvent;
class QDropEvent;
class QAction;

namespace bitxray::ui {

class NaluListModel;
class HexView;
class SyntaxTreeModel;
class VideoPreviewWidget;
class PlaybackWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Exposed so `main()` can support `bitxray_ui <path>` for quick manual
    // testing without going through the Open file dialog each time.
    void loadFile(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void openFile();
    void onCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous);
    void onVideoFrameChanged(int naluIndex);
    void showAboutDialog();
    void openPlaybackWindow();

private:
    QByteArray fileData_;
    NaluListModel* naluListModel_;
    QTableView* naluTable_;
    HexView* hexView_;
    SyntaxTreeModel* syntaxTree_;
    VideoPreviewWidget* videoPreview_;
    PlaybackWindow* playbackWindow_;
    QAction* playAction_;
};

} // namespace bitxray::ui

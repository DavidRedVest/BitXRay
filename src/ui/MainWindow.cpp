#include "MainWindow.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QMimeData>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QToolBar>
#include <QUrl>

#include "HexView.h"
#include "NaluListModel.h"
#include "PlaybackWindow.h"
#include "SyntaxTreeModel.h"
#include "VideoPreviewWidget.h"

namespace bitxray::ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      naluListModel_(new NaluListModel(this)),
      naluTable_(new QTableView(this)),
      hexView_(new HexView(this)),
      syntaxTree_(new SyntaxTreeModel(this)),
      videoPreview_(new VideoPreviewWidget(this)),
      playbackWindow_(nullptr),
      playAction_(nullptr) {
    setWindowTitle(QStringLiteral("BitXRay"));
    setWindowIcon(QIcon(QStringLiteral(":/resources/ProtocolAnalysis.svg")));
    resize(1400, 900);
    setAcceptDrops(true);

    naluTable_->setModel(naluListModel_);
    naluTable_->horizontalHeader()->setStretchLastSection(true);
    naluTable_->verticalHeader()->setVisible(false); // redundant with our own "No." column
    naluTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    naluTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    naluTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* leftSplitter = new QSplitter(Qt::Vertical, this);
    leftSplitter->addWidget(naluTable_);
    leftSplitter->addWidget(videoPreview_);
    leftSplitter->setStretchFactor(0, 2);
    leftSplitter->setStretchFactor(1, 1);

    auto* rightSplitter = new QSplitter(Qt::Vertical, this);
    rightSplitter->addWidget(hexView_);
    rightSplitter->addWidget(syntaxTree_);
    rightSplitter->setStretchFactor(0, 1);
    rightSplitter->setStretchFactor(1, 1);

    auto* mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(leftSplitter);
    mainSplitter->addWidget(rightSplitter);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 1);
    setCentralWidget(mainSplitter);

    connect(naluTable_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            &MainWindow::onCurrentRowChanged);
    connect(videoPreview_, &VideoPreviewWidget::frameChanged, this,
            &MainWindow::onVideoFrameChanged);

    QAction* openAction = new QAction(QStringLiteral("&Open..."), this);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setToolTip(QStringLiteral("Open an H.264/H.265 elementary stream"));
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    playAction_ = new QAction(QStringLiteral("&Play"), this);
    playAction_->setToolTip(QStringLiteral("Open a window and play the whole video"));
    playAction_->setEnabled(false);
    connect(playAction_, &QAction::triggered, this, &MainWindow::openPlaybackWindow);

    QAction* aboutAction = new QAction(QStringLiteral("&About"), this);
    aboutAction->setToolTip(QStringLiteral("About BitXRay"));
    // Without this, macOS/Qt auto-detects "About"-looking actions by text
    // and relocates them into the native app menu — not where a directly
    // visible toolbar button is supposed to end up.
    aboutAction->setMenuRole(QAction::NoRole);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    // Deliberately no QMenuBar menu here: Open/Play/About live only on the
    // toolbar below. On macOS a menu bar is easy to overlook (it just merges
    // into the system-wide menu bar), but on Windows/Linux it renders as a
    // literal extra "File" dropdown row duplicating the toolbar buttons —
    // exactly the menu clutter this toolbar-only layout was chosen to avoid.
    QToolBar* toolBar = addToolBar(QStringLiteral("Main"));
    toolBar->setMovable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->addAction(openAction);
    toolBar->addAction(playAction_);
    toolBar->addAction(aboutAction);

    statusBar()->showMessage(
        QStringLiteral("Open (toolbar/Cmd+O) or drag & drop an H.264/H.265 elementary stream to begin."));
}

namespace {

bool looksLikeElementaryStream(const QString& path) {
    static const QStringList kExtensions = {QStringLiteral("h264"), QStringLiteral("264"),
                                             QStringLiteral("h265"), QStringLiteral("265"),
                                             QStringLiteral("hevc")};
    const QString suffix = QFileInfo(path).suffix().toLower();
    return kExtensions.contains(suffix);
}

} // namespace

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile() && looksLikeElementaryStream(url.toLocalFile())) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && looksLikeElementaryStream(url.toLocalFile())) {
            loadFile(url.toLocalFile());
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::openFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open Elementary Stream"), QString(),
        QStringLiteral("H.264/H.265 streams (*.h264 *.264 *.h265 *.265 *.hevc);;All files (*)"));
    if (!path.isEmpty()) {
        loadFile(path);
    }
}

void MainWindow::loadFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("BitXRay"),
                              QStringLiteral("Could not open file:\n%1").arg(path));
        return;
    }
    fileData_ = file.readAll();

    // Parsing + the video engine's up-front decode-order-learning pass (see
    // VideoDecodeEngine::loadStream) are both synchronous and can take a
    // noticeable moment on a large file — an override cursor is cheap
    // insurance against that reading as a hang rather than a load in
    // progress. processEvents() forces the cursor change to actually paint
    // before the blocking work below starts.
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QGuiApplication::processEvents();

    naluListModel_->load(reinterpret_cast<const uint8_t*>(fileData_.constData()),
                          static_cast<std::size_t>(fileData_.size()));
    hexView_->setFileData(reinterpret_cast<const uint8_t*>(fileData_.constData()),
                           static_cast<std::size_t>(fileData_.size()));
    hexView_->clear();
    syntaxTree_->showMessage(QStringLiteral("Select an SPS/PPS row to view its syntax elements."));
    videoPreview_->loadStream(naluListModel_->naluInfos(), fileData_);
    playAction_->setEnabled(videoPreview_->frameCount() > 0);

    QGuiApplication::restoreOverrideCursor();

    statusBar()->showMessage(
        QStringLiteral("Loaded %1 (%2 bytes, %3 NALUs)")
            .arg(path)
            .arg(fileData_.size())
            .arg(naluListModel_->rowCount()));
}

void MainWindow::onCurrentRowChanged(const QModelIndex& current, const QModelIndex& /*previous*/) {
    if (!current.isValid()) {
        hexView_->clear();
        syntaxTree_->showMessage(QString());
        return;
    }

    const int row = current.row();
    const NaluInfo* info = naluListModel_->naluAt(row);
    if (!info) {
        return;
    }
    hexView_->showRange(info->offset, info->length);

    if (auto sps = naluListModel_->h264SpsAt(row)) {
        syntaxTree_->showH264Sps(*sps);
    } else if (auto pps = naluListModel_->h264PpsAt(row)) {
        syntaxTree_->showH264Pps(*pps);
    } else if (auto h265sps = naluListModel_->h265SpsAt(row)) {
        syntaxTree_->showH265Sps(*h265sps);
    } else if (auto h265pps = naluListModel_->h265PpsAt(row)) {
        syntaxTree_->showH265Pps(*h265pps);
    } else if (auto sliceDetail = naluListModel_->h264SliceDetailAt(row)) {
        syntaxTree_->showH264SliceHeader(naluListModel_->h264ForbiddenZeroBitAt(row),
                                          naluListModel_->h264NalRefIdcAt(row), info->naluType,
                                          *sliceDetail);
    } else if (auto h265SliceDetail = naluListModel_->h265SliceDetailAt(row)) {
        syntaxTree_->showH265SliceHeader(
            naluListModel_->h265ForbiddenZeroBitAt(row), naluListModel_->h265NuhLayerIdAt(row),
            naluListModel_->h265NuhTemporalIdPlus1At(row), info->naluType, *h265SliceDetail);
    } else {
        syntaxTree_->showGenericNalu(*info, naluListModel_->extraInfoAt(row));
    }

    // Core innovation (design doc 3.2): clicking an I/P/B slice row seeks
    // the player to render that exact frame. Non-slice rows (SPS/PPS/SEI/
    // AUD) don't correspond to a displayable picture, so leave the player
    // as-is for those.
    if (info->isSlice()) {
        videoPreview_->goToNalu(row);
    }
}

void MainWindow::openPlaybackWindow() {
    if (!playbackWindow_) {
        playbackWindow_ = new PlaybackWindow(this);
    }
    playbackWindow_->loadStream(naluListModel_->naluInfos(), fileData_);
    playbackWindow_->show();
    playbackWindow_->raise();
    playbackWindow_->activateWindow();
    playbackWindow_->play();
}

void MainWindow::showAboutDialog() {
    QMessageBox::about(
        this, QStringLiteral("About BitXRay"),
        QStringLiteral(
            "<h3>BitXRay</h3>"
            "<p>Version 0.1.1 (built %1)</p>"
            "<p>An H.264/H.265 elementary-stream analyzer and visualizer — NALU/hex/syntax-tree "
            "inspection kept in sync with frame-accurate video playback.</p>"
            "<p>Author: Lichance</p>")
            .arg(QStringLiteral(__DATE__)));
}

void MainWindow::onVideoFrameChanged(int naluIndex) {
    // The reverse direction: as the player advances (via Play/Next/Prev or
    // the seek above), auto-scroll and highlight the NALU list to match.
    // Row indices in naluListModel_ are exactly the NALU indices passed to
    // VideoPreviewWidget::loadStream(), so no translation is needed. Qt's
    // QItemSelectionModel doesn't re-emit currentRowChanged when the index
    // is already current, so this can't loop back into
    // onCurrentRowChanged/goToNalu indefinitely.
    const QModelIndex index = naluListModel_->index(naluIndex, 0);
    if (!index.isValid()) {
        return;
    }
    naluTable_->selectionModel()->setCurrentIndex(
        index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    naluTable_->scrollTo(index);
}

} // namespace bitxray::ui

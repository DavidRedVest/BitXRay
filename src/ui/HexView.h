#pragma once

#include <QWidget>
#include <cstddef>
#include <cstdint>

class QPlainTextEdit;

namespace bitxray::ui {

// Renders a byte range as classic hex+ASCII columns, synced to whatever
// NALU is selected in the list. Does not own the underlying file bytes —
// MainWindow keeps the loaded file alive for as long as the view might
// need it.
class HexView : public QWidget {
    Q_OBJECT

public:
    explicit HexView(QWidget* parent = nullptr);

    void setFileData(const uint8_t* data, std::size_t size);
    void showRange(std::size_t offset, std::size_t length);
    void clear();

private:
    QPlainTextEdit* textEdit_;
    const uint8_t* fileData_ = nullptr;
    std::size_t fileSize_ = 0;
};

} // namespace bitxray::ui

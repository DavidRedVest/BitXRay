#include "HexView.h"

#include <algorithm>

#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QString>
#include <QVBoxLayout>

namespace bitxray::ui {

HexView::HexView(QWidget* parent) : QWidget(parent), textEdit_(new QPlainTextEdit(this)) {
    textEdit_->setReadOnly(true);
    textEdit_->setLineWrapMode(QPlainTextEdit::NoWrap);
    textEdit_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(textEdit_);
}

void HexView::setFileData(const uint8_t* data, std::size_t size) {
    fileData_ = data;
    fileSize_ = size;
}

void HexView::clear() {
    textEdit_->clear();
}

void HexView::showRange(std::size_t offset, std::size_t length) {
    if (!fileData_ || offset >= fileSize_) {
        clear();
        return;
    }
    const std::size_t end = std::min(offset + length, fileSize_);

    QString text;
    text.reserve(static_cast<int>((end - offset) / 16 + 1) * 80);

    for (std::size_t rowStart = offset; rowStart < end; rowStart += 16) {
        const std::size_t rowEnd = std::min(rowStart + 16, end);

        text += QStringLiteral("%1  ").arg(rowStart, 8, 16, QLatin1Char('0'));

        QString ascii;
        for (std::size_t i = rowStart; i < rowStart + 16; ++i) {
            if (i < rowEnd) {
                const uint8_t byte = fileData_[i];
                text += QStringLiteral("%1 ").arg(byte, 2, 16, QLatin1Char('0'));
                ascii += (byte >= 0x20 && byte < 0x7f) ? QChar(static_cast<char>(byte))
                                                        : QChar('.');
            } else {
                text += QStringLiteral("   ");
            }
            if (i - rowStart == 7) {
                text += QStringLiteral(" ");
            }
        }
        text += QStringLiteral(" ") + ascii + QStringLiteral("\n");
    }

    textEdit_->setPlainText(text);
}

} // namespace bitxray::ui

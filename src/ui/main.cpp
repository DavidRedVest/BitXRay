#include <QApplication>
#include <QIcon>

#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/resources/ProtocolAnalysis.svg")));

    bitxray::ui::MainWindow window;
    if (argc > 1) {
        window.loadFile(QString::fromLocal8Bit(argv[1]));
    }

    window.show();

    return app.exec();
}

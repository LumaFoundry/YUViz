#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include "mywindow.h"
#include "decoder/videoDecoder.h"
#include "controller/frameController.h"
#include "rendering/videoRenderer.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    if (argc != 4) {
        QMessageBox::critical(nullptr, "Error", "Usage: <YUV file path> <width> <height>");
        return -1;
    }

    QString yuvFilePath = argv[1];
    int width = QString(argv[2]).toInt();
    int height = QString(argv[3]).toInt();
    int size = width * height * 3 / 2; // YUV 4:2:0 format

    // MyWindow window;
    if (!QFile::exists(yuvFilePath)) {
        QMessageBox::critical(nullptr, "Error", "YUV file does not exist.");
        return -1;
    }
    VideoDecoder decoder = nullptr;
    decoder.setFileName(yuvFilePath.toStdString());
    decoder.setDimensions(width, height);
    decoder.openFile();

    VideoRenderer renderer;

    FrameController frameController(nullptr, &decoder, &renderer);

    // // TODO: More checks (e.g., file format, dimensions)

    // window.show();

    return app.exec();
}
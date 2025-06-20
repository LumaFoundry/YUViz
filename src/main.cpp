#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include "mywindow.h"
#include "decoder/videoDecoder.h"
#include "controller/frameController.h"
#include "controller/videoController.h"
#include "rendering/videoRenderer.h"
#include <QCommandLineParser>
#include <QCommandLineOption>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCommandLineParser parser;

    parser.setApplicationDescription("Visual Inspection Tool");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("video",
        "One or more videos with their width and height",
        "<video_name> <width> <height> [<video_name> <width> <height>...]");
    parser.process(app);

    const QStringList args = parser.positionalArguments();

    // Check if we have at least one video (needs 3 arguments per video)
    if (args.size() < 3 || args.size() % 3 != 0) {
        parser.showHelp(-1);
    }

    auto playbackWorker = std::make_shared<PlaybackWorker>();
    VideoController videoController(nullptr, playbackWorker);

    int numVideos = args.size() / 3;
    
    for (int i = 0; i < numVideos; i++) {
        int argIndex = i * 3;
        
        QString yuvFilePath = args[argIndex];
        bool ok1, ok2;
        int width = args[argIndex + 1].toInt(&ok1);
        int height = args[argIndex + 2].toInt(&ok2);

        if (!ok1 || !ok2) {
            QMessageBox::critical(nullptr, "Error", 
                QString("Invalid dimensions for video: %1").arg(yuvFilePath));
            return -1;
        }

        if (!QFile::exists(yuvFilePath)) {
            QMessageBox::critical(nullptr, "Error", 
                QString("YUV file does not exist: %1").arg(yuvFilePath));
            return -1;
        }

        auto decoder = new VideoDecoder();
        decoder->setFileName(yuvFilePath.toStdString());
        decoder->setDimensions(width, height);
        decoder->openFile();

        auto renderer = new VideoRenderer();

        auto frameController = new FrameController(nullptr, decoder, renderer, playbackWorker, i);

        videoController.addFrameController(frameController);
    }

    // Start all FC - IMPORTANT: This must be done after all FCs are added !
    for (auto* fc : videoController.getFrameControllers()) {
        fc->start();
    }

    // TODO: Create and show window

    return app.exec();
}

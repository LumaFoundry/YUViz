#include <QGuiApplication>
#include <QWindow>
#include <QSurface>
#include <QTimer>
#include <QFile>
#include <QMessageBox>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <memory>
#include "decoder/videoDecoder.h"
#include "controller/frameController.h"
#include "controller/videoController.h"
#include "rendering/videoRenderer.h"
#include "ui/videoWindow.h"


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Code for argument parsing and selection of graphics API taken
    // from qtbase/examples/gui/rhiwindow/
    QRhi::Implementation graphicsApi;

    // Use platform-specific defaults when no command-line arguments given.
#if defined(Q_OS_WIN)
    graphicsApi = QRhi::D3D11;
#elif QT_CONFIG(metal)
    graphicsApi = QRhi::Metal;
#elif QT_CONFIG(vulkan) && defined(USE_VULKAN)
    graphicsApi = QRhi::Vulkan;
#else
    graphicsApi = QRhi::OpenGLES2;
#endif

    QCommandLineParser parser;

    parser.setApplicationDescription("Visual Inspection Tool");
    parser.addVersionOption();

    parser.addHelpOption();
    QCommandLineOption nullOption({ "n", "null" }, QLatin1String("Null"));
    parser.addOption(nullOption);
    QCommandLineOption glOption({ "g", "opengl" }, QLatin1String("OpenGL"));
    parser.addOption(glOption);
    QCommandLineOption vkOption({ "v", "vulkan" }, QLatin1String("Vulkan"));
    parser.addOption(vkOption);
    QCommandLineOption d3d11Option({ "d", "d3d11" }, QLatin1String("Direct3D 11"));
    parser.addOption(d3d11Option);
    QCommandLineOption d3d12Option({ "D", "d3d12" }, QLatin1String("Direct3D 12"));
    parser.addOption(d3d12Option);
    QCommandLineOption mtlOption({ "m", "metal" }, QLatin1String("Metal"));
    parser.addOption(mtlOption);

    parser.addPositionalArgument("video",
        "One or more videos with their width and height",
        "<video_name> <width> <height> [<video_name> <width> <height>...]");
    parser.process(app);

    const QStringList args = parser.positionalArguments();

    // Check if we have at least one video (needs 3 arguments per video)
    if (args.size() < 4 || args.size() % 4 != 0) {
        parser.showHelp(-1);
    }

    auto playbackWorker = std::make_shared<PlaybackWorker>();
    VideoController videoController(nullptr, playbackWorker);

    int numVideos = (args.size() - 1) / 3;
    
    // TODO: Not a good idea to read args
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

        auto decoder = std::make_unique(VideoDecoder());
        decoder->setFileName(yuvFilePath.toStdString());
        decoder->setDimensions(width, height);
        decoder->openFile();

        // TODO: Move window and renderer to videoController
        auto window = std::make_unique(VideoWindow(nullptr, graphicsApi));
        auto renderer = std::make_unique(VideoRenderer(&window, decoder->getMetaData()));

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

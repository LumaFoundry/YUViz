#include <QApplication>
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

    QCommandLineOption graphicsApiOption(
        QStringList() << "g" << "graphics",
        QLatin1String("Select the graphics API to use. Supported values: opengl, vulkan, d3d11, d3d12, metal, null."),
        QLatin1String("api"));
    parser.addOption(graphicsApiOption);

    parser.addPositionalArgument("video",
        "One or more videos with their width and height",
        "<video_name> <width> <height> [<video_name> <width> <height>...]");
    parser.process(app);

    // Check if the user specified a graphics API
    if (parser.isSet(graphicsApiOption)) {
        const QString api = parser.value(graphicsApiOption).toLower();
        if (api == QLatin1String("opengl")) {
            graphicsApi = QRhi::OpenGLES2;
        } else if (api == QLatin1String("vulkan")) {
            graphicsApi = QRhi::Vulkan;
        } else if (api == QLatin1String("d3d11")) {
            graphicsApi = QRhi::D3D11;
        } else if (api == QLatin1String("d3d12")) {
            graphicsApi = QRhi::D3D12;
        } else if (api == QLatin1String("metal")) {
            graphicsApi = QRhi::Metal;
        } else if (api == QLatin1String("null")) {
            graphicsApi = QRhi::Null;
        } else {
            qWarning("Unknown graphics API '%s' specified. Falling back to the default.", qPrintable(api));
        }
    }

    const QStringList args = parser.positionalArguments();

    // TODO: Need a better safe guard to check arguments
    // Check if we have at least one video (needs 3 arguments per video)
    // if (args.size() < 3 || args.size() % 4 != 0) {
    //     parser.showHelp(-1);
    // }

    auto playbackWorker = std::make_shared<PlaybackWorker>();
    VideoController videoController(nullptr, playbackWorker);

    int numVideos = 1;
    
    // TODO: Not a good idea to read args
    // TODO: Use flags to take argument inputs from command line (-f for file, -w for width, -h for height)
    // TODO: Move creation of classes to videoController

    // for (int i = 0; i < numVideos; i++) {
    //     int argIndex = i * 3;
        
    //     QString yuvFilePath = args[argIndex];
    //     bool ok1, ok2;
    //     int width = args[argIndex + 1].toInt(&ok1);
    //     int height = args[argIndex + 2].toInt(&ok2);

    //     if (!ok1 || !ok2) {
    //         QMessageBox::critical(nullptr, "Error", 
    //             QString("Invalid dimensions for video: %1").arg(yuvFilePath));
    //         return -1;
    //     }

    //     if (!QFile::exists(yuvFilePath)) {
    //         QMessageBox::critical(nullptr, "Error", 
    //             QString("YUV file does not exist: %1").arg(yuvFilePath));
    //         return -1;
    //     }

    //     auto decoder = std::make_unique<VideoDecoder>();
    //     decoder->setFileName(yuvFilePath.toStdString());
    //     decoder->setDimensions(width, height);
    //     decoder->openFile();
    //     std::shared_ptr<FrameMeta> metaPtr = std::make_shared<FrameMeta>(decoder->getMetaData());

    //     // TODO: Move window and renderer to videoController
    //     auto windowPtr = std::make_shared<VideoWindow>(nullptr, graphicsApi);
    //     auto renderer = std::make_unique<VideoRenderer>(nullptr, windowPtr, metaPtr);

    //     auto frameController = std::make_unique<FrameController>(nullptr, decoder.get(), renderer.get(), playbackWorker, i);

    //     videoController.addFrameController(frameController.get());
    // }


    // Start all FC - IMPORTANT: This must be done after all FCs are added !
    for (auto* fc : videoController.getFrameControllers()) {
        fc->start();
    }

    // TODO: Create and show window

    return app.exec();
}

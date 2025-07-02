#include <QApplication>
#include <QWindow>
#include <QSurface>
#include <QTimer>
#include <QFile>
#include <QDebug>
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
    qDebug() << "Application starting with arguments:" << app.arguments();
    QRhi::Implementation graphicsApi;

    // Use platform-specific defaults when no command-line arguments given.
#if defined(Q_OS_WIN)
    graphicsApi = QRhi::D3D11;
#elif QT_CONFIG(metal)
    graphicsApi = QRhi::Metal;
#elif QT_CONFIG(vulkan) && defined(VULKAN_INSTALLED)
    graphicsApi = QRhi::Vulkan;
#else
    graphicsApi = QRhi::OpenGLES2;
#endif

    // Set QDebug output to be off by default
    qSetMessagePattern("");

    QCommandLineParser parser;

    parser.setApplicationDescription("Visual Inspection Tool");
    parser.addVersionOption();
    parser.addHelpOption();
    parser.addPositionalArgument("file", "File path");

    QCommandLineOption graphicsApiOption(
        QStringList() << "g" << "graphics",
        QLatin1String("Select the graphics API to use. Supported values: opengl, vulkan, d3d11, d3d12, metal, null."),
        QLatin1String("api"));
    parser.addOption(graphicsApiOption);

    int width = 1920;
    int height = 1080;
    double framerate = 25.0;

    QCommandLineOption debugOption({"d", "debug"}, "Enable debug output");
    parser.addOption(debugOption);
    QCommandLineOption framerateOption({"f", "framerate"}, QLatin1String("Framerate"), QLatin1String("framerate"));
    parser.addOption(framerateOption);
    QCommandLineOption resolutionOption({"r", "resolution"}, QLatin1String("Video resolution"), QLatin1String("resolution"));
    parser.addOption(resolutionOption);
    parser.process(app);

    const QStringList args = parser.positionalArguments();

    if (parser.isSet(debugOption)) {
        qSetMessagePattern("[%{type}] %{message}");
    }

    if (parser.isSet(framerateOption)) {
        parser.value(framerateOption).toDouble();
    }

    if (parser.isSet(resolutionOption)) {
        bool ok1, ok2;
        width = parser.value(resolutionOption).split("x")[0].toInt(&ok1);
        height = parser.value(resolutionOption).split("x")[1].toInt(&ok2);
        if (!ok1 || !ok2) {
            qWarning() << "Invalid dimensions for video:" << parser.value(resolutionOption);
            QMessageBox::critical(nullptr, "Error", QString("Invalid dimensions for video: %1 x %2").arg(parser.value(resolutionOption).split("x")[0]).arg(parser.value(resolutionOption).split("x")[1]));
            return -1;
        }
    }

    QString filename = args.first();

    qDebug() << "Parsed command-line options. File: " << parser.value(filename)
             << "Resolution: " << parser.value(resolutionOption)
             << "Framerate: " << parser.value(framerateOption);



    qDebug() << "Video file path:" << filename
             << "Width:" << width << "Height:" << height;


    if (!QFile::exists(filename)) {
        qWarning() << "YUV file does not exist:" << filename;
        QMessageBox::critical(nullptr, "Error", QString("YUV file does not exist: %1").arg(filename));
        return -1;
    }

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
    qDebug() << "Using graphics API enum value:" << static_cast<int>(graphicsApi);

    // TODO: Need a better safe guard to check arguments
    // Check if we have at least one video (needs 3 arguments per video)
    // if (args.size() < 3 || args.size() % 4 != 0) {
    //     parser.showHelp(-1);
    // }

    // TODO: This can be used to handle multiple videos later
    VideoFileInfo videoFileInfo;
    videoFileInfo.filename = filename;
    videoFileInfo.width = width;
    videoFileInfo.height = height;
    videoFileInfo.graphicsApi = graphicsApi;
    videoFileInfo.framerate = framerate;

    std::vector<VideoFileInfo> videoFiles;
    videoFiles.push_back(videoFileInfo);
    qDebug() << "Number of video files to play:" << videoFiles.size();

    qDebug() << "Creating PlaybackWorker";
    auto playbackWorker = std::make_shared<PlaybackWorker>();
    qDebug() << "PlaybackWorker created successfully";
    qDebug() << "Creating VideoController";
    VideoController videoController(nullptr, playbackWorker, videoFiles);
    qDebug() << "VideoController created successfully";

    // TODO: Create and show window

    return app.exec();
}

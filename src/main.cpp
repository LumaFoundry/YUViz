#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSurface>
#include <QTimer>
#include <QWindow>
#include <memory>
#include "controller/frameController.h"
#include "controller/videoController.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "ui/videoWindow.h"
#include "utils/videoFileInfo.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    qDebug() << "Application starting with arguments:" << app.arguments();

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
    QCommandLineOption resolutionOption(
        {"r", "resolution"}, QLatin1String("Video resolution"), QLatin1String("resolution"));
    parser.addOption(resolutionOption);
    parser.process(app);

    if (parser.isSet(debugOption)) {
        qSetMessagePattern("[%{type}] %{message}");
    }

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
            QMessageBox::critical(nullptr,
                                  "Error",
                                  QString("Invalid dimensions for video: %1 x %2")
                                      .arg(parser.value(resolutionOption).split("x")[0])
                                      .arg(parser.value(resolutionOption).split("x")[1]));
            return -1;
        }
    }

    QString filename = args.first();

    qDebug() << "Parsed command-line options. File: " << parser.value(filename)
             << "Resolution: " << parser.value(resolutionOption) << "Framerate: " << parser.value(framerateOption);

    qDebug() << "Video file path:" << filename << "Width:" << width << "Height:" << height
             << "Framerate: " << parser.value(framerateOption);

    if (!QFile::exists(filename)) {
        qWarning() << "YUV file does not exist:" << filename;
        QMessageBox::critical(nullptr, "Error", QString("YUV file does not exist: %1").arg(filename));
        return -1;
    }

    // TODO: Need a better safe guard to check arguments
    // Check if we have at least one video (needs 3 arguments per video)
    // if (args.size() < 3 || args.size() % 4 != 0) {
    //     parser.showHelp(-1);
    // }

    // TODO: Create and show window
    QQmlApplicationEngine engine;

    qmlRegisterType<VideoWindow>("Window", 1, 0, "VideoWindow");

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.load(url);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    QObject* root = engine.rootObjects().first();
    auto* windowPtr = root->findChild<VideoWindow*>("videoWindow");

    windowPtr->setAspectRatio(width, height);

    // TODO: This can be used to handle multiple videos later
    VideoFileInfo videoFileInfo;
    videoFileInfo.filename = filename;
    videoFileInfo.width = width;
    videoFileInfo.height = height;
    videoFileInfo.framerate = framerate;
    videoFileInfo.windowPtr = windowPtr;

    std::vector<VideoFileInfo> videoFiles;
    videoFiles.push_back(videoFileInfo);
    qDebug() << "Number of video files to play:" << videoFiles.size();

    qDebug() << "Creating VideoController";
    VideoController videoController(nullptr, videoFiles);
    qDebug() << "VideoController created successfully";

    videoController.start();

    engine.rootContext()->setContextProperty("videoController", &videoController);

    return app.exec();
}

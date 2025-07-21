#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSurface>
#include <QTimer>
#include <QWindow>
#include <memory>
#include <iostream>
#include "controller/frameController.h"
#include "controller/videoController.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "ui/videoWindow.h"
#include "utils/videoFileInfo.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
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
    AVPixelFormat yuvFormat = AV_PIX_FMT_YUV420P; // Default to 420

    QCommandLineOption debugOption({"d", "debug"}, "Enable debug output");
    parser.addOption(debugOption);
    QCommandLineOption framerateOption({"f", "framerate"}, QLatin1String("Framerate"), QLatin1String("framerate"));
    parser.addOption(framerateOption);
    QCommandLineOption resolutionOption(
        {"r", "resolution"}, QLatin1String("Video resolution"), QLatin1String("resolution"));
    parser.addOption(resolutionOption);
    QCommandLineOption yuvFormatOption(
        {"y", "yuv-format"}, QLatin1String("YUV pixel format"), QLatin1String("format"));
    parser.addOption(yuvFormatOption);
    parser.process(app);

    if (parser.isSet(debugOption)) {
        qSetMessagePattern("[%{type}] %{message}");
    }

    const QStringList args = parser.positionalArguments();

    if (parser.isSet(debugOption)) {
        qSetMessagePattern("[%{type}] %{message}");
    }

    if (parser.isSet(framerateOption)) {
        framerate = parser.value(framerateOption).toDouble();
    }

    if (parser.isSet(resolutionOption)) {
        bool ok1, ok2;
        width = parser.value(resolutionOption).split("x")[0].toInt(&ok1);
        height = parser.value(resolutionOption).split("x")[1].toInt(&ok2);
        if (!ok1 || !ok2) {
            qWarning() << "Invalid dimensions for video:" << parser.value(resolutionOption);
            std::cerr << "Error: Invalid dimensions for video: " 
                      << parser.value(resolutionOption).split("x")[0].toStdString() 
                      << " x " 
                      << parser.value(resolutionOption).split("x")[1].toStdString() << std::endl;
            return -1;
        }
    }

    if (parser.isSet(yuvFormatOption)) {
        QString yuvFormatStr = parser.value(yuvFormatOption);
        if (yuvFormatStr == "420P") {
            yuvFormat = AV_PIX_FMT_YUV420P;
        } else if (yuvFormatStr == "422P") {
            yuvFormat = AV_PIX_FMT_YUV422P;
        } else if (yuvFormatStr == "444P") {
            yuvFormat = AV_PIX_FMT_YUV444P;
        } else {
            std::cerr << "Error: Invalid YUV format: " << yuvFormatStr.toStdString() 
                      << ". Supported formats: 420P, 422P, 444P" << std::endl;
            return -1;
        }
    }

    if (args.isEmpty()) {
        qWarning() << "No file specified";
        std::cerr << "Error: No file specified" << std::endl;
        parser.showHelp(-1);
    }

    QString filename = args.first();

    // Check if the file is a .yuv file and require specific flags
    if (filename.toLower().endsWith(".yuv")) {
        bool hasAllFlags = parser.isSet(framerateOption) && 
                          parser.isSet(resolutionOption) && 
                          parser.isSet(yuvFormatOption);
        
        if (!hasAllFlags) {
            qWarning() << "For .yuv files, all required flags must be specified";
            std::cerr << "Error: For .yuv files, the following flags are required: -f/--framerate, -r/--resolution, -y/--yuv-format" << std::endl;
            return -1;
        }
    }

    qDebug() << "Parsed command-line options. File: " << parser.value(filename)
             << "Resolution: " << parser.value(resolutionOption) << "Framerate: " << parser.value(framerateOption);

    qDebug() << "Video file path:" << filename << "Width:" << width << "Height:" << height
             << "Framerate: " << parser.value(framerateOption);

    if (!QFile::exists(filename)) {
        qWarning() << "File does not exist:" << filename;
        std::cerr << "Error: File does not exist: " << filename.toStdString() << std::endl;
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

    std::vector<VideoFileInfo> videoFiles;

    // === Test Code - please remove later === //
    int videoCount = 1;
    // === Test Code - please remove later === //

    engine.rootContext()->setContextProperty("videoCount", videoCount);

    QObject* root = engine.rootObjects().first();

    for (int i = 0; i < videoCount; i++) {
        QString windowName = QString("videoWindow_%1").arg(i);

        auto* windowPtr = root->findChild<VideoWindow*>(windowName);

        if (!windowPtr) {
            qDebug() << "windowPtr is NULL for window name:" << windowName;
        }
        windowPtr->setAspectRatio(width, height);

        VideoFileInfo videoFileInfo;
        videoFileInfo.filename = filename;
        videoFileInfo.width = width;
        videoFileInfo.height = height;
        videoFileInfo.pixelFormat = yuvFormat;
        // videoFileInfo.framerate = framerate;
        videoFileInfo.windowPtr = windowPtr;

        // === Test Code - please remove later === //
        videoFileInfo.framerate = (i == 0) ? framerate : 24.0;
        // === Test Code - please remove later === //

        videoFiles.push_back(videoFileInfo);
    }

    qDebug() << "Number of video files to play:" << videoFiles.size();

    qDebug() << "Creating VideoController";
    VideoController videoController(nullptr, videoFiles);
    qDebug() << "VideoController created successfully";

    videoController.start();

    engine.rootContext()->setContextProperty("videoController", &videoController);

    return app.exec();
}

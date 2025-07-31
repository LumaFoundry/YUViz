#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSurface>
#include <QTimer>
#include <QWindow>
#include <iostream>
#include <memory>
#include "controller/compareController.h"
#include "controller/videoController.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "ui/videoLoader.h"
#include "ui/videoWindow.h"
#include "utils/appConfig.h"
#include "utils/sharedViewProperties.h"
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

    int width = 1920;
    int height = 1080;
    double framerate = 25.0;
    QString yuvFormat = "420P"; // Default to 420

    QCommandLineOption debugOption({"d", "debug"}, "Enable debug output");
    parser.addOption(debugOption);

    QCommandLineOption framerateOption({"f", "framerate"}, QLatin1String("Framerate"), QLatin1String("framerate"));
    parser.addOption(framerateOption);

    QCommandLineOption resolutionOption(
        {"r", "resolution"}, QLatin1String("Video resolution"), QLatin1String("resolution"));
    parser.addOption(resolutionOption);

    QCommandLineOption yuvFormatOption({"y", "yuv-format"}, QLatin1String("YUV pixel format"), QLatin1String("format"));
    parser.addOption(yuvFormatOption);

    QCommandLineOption queueSizeOption({"q", "queue-size"}, QLatin1String("Frame queue size"), QLatin1String("size"));
    parser.addOption(queueSizeOption);

    parser.process(app);
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
            ErrorReporter::instance().report(
                QString("Invalid dimensions for video: %1").arg(parser.value(resolutionOption)), LogLevel::Error);
            return -1;
        }
    }

    if (parser.isSet(yuvFormatOption)) {
        yuvFormat = parser.value(yuvFormatOption);
        if (yuvFormat != "420P" && yuvFormat != "422P" && yuvFormat != "444P") {
            qWarning() << "Invalid YUV format:" << yuvFormat;
            ErrorReporter::instance().report(QString("Invalid YUV format: %1").arg(yuvFormat), LogLevel::Error);
            return -1;
        }
    }

    // Parse queue size option
    if (parser.isSet(queueSizeOption)) {
        bool ok;
        int queueSize = parser.value(queueSizeOption).toInt(&ok);
        if (!ok || queueSize <= 0) {
            qWarning() << "Invalid queue size:" << parser.value(queueSizeOption);
            ErrorReporter::instance().report(QString("Invalid queue size: %1").arg(parser.value(queueSizeOption)),
                                             LogLevel::Error);
            return -1;
        }
        AppConfig::instance().setQueueSize(queueSize);
        qDebug() << "Setting frame queue size to:" << queueSize;
    }

    QQmlApplicationEngine engine;

    SharedViewProperties sharedViewProperties;
    engine.rootContext()->setContextProperty("sharedViewProperties", &sharedViewProperties);

    qmlRegisterSingletonType(QUrl("qrc:/Theme.qml"), "Theme", 1, 0, "Theme");
    qmlRegisterType<VideoWindow>("VideoWindow", 1, 0, "VideoWindow");
    qmlRegisterType<DiffWindow>("DiffWindow", 1, 0, "DiffWindow");

    std::shared_ptr<CompareController> compareController = std::make_shared<CompareController>(nullptr);
    std::shared_ptr<VideoController> videoController = std::make_shared<VideoController>(nullptr, compareController);
    VideoLoader videoLoader(&engine, nullptr, videoController, compareController, &sharedViewProperties);

    std::vector<VideoFileInfo> videoFiles;
    engine.rootContext()->setContextProperty("videoLoader", &videoLoader);
    engine.rootContext()->setContextProperty("compareController", compareController.get());
    engine.rootContext()->setContextProperty("videoController", videoController.get());

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.load(url);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    if (!args.isEmpty()) {
        QString filename = args.first();

        if (filename.toLower().endsWith(".yuv")) {
            bool hasAllFlags =
                parser.isSet(framerateOption) && parser.isSet(resolutionOption) && parser.isSet(yuvFormatOption);

            if (!hasAllFlags) {
                qWarning() << "For .yuv files, all required flags must be specified";
                ErrorReporter::instance().report("For .yuv files, all required flags must be specified",
                                                 LogLevel::Error);
                return -1;
            }
        }

        qDebug() << "Parsed command-line options. File: " << filename
                 << "Resolution: " << parser.value(resolutionOption) << "Framerate: " << parser.value(framerateOption);

        if (!QFile::exists(filename)) {
            qWarning() << "YUV file does not exist:" << filename;
            ErrorReporter::instance().report(QString("YUV file does not exist: %1").arg(filename), LogLevel::Error);
            return -1;
        }

        // Delay property update until after QML is loaded
        QObject* root = engine.rootObjects().first();
        QMetaObject::invokeMethod(root,
                                  "importVideoFromParams",
                                  Qt::QueuedConnection,
                                  Q_ARG(QVariant, filename),
                                  Q_ARG(QVariant, width),
                                  Q_ARG(QVariant, height),
                                  Q_ARG(QVariant, framerate),
                                  Q_ARG(QVariant, yuvFormat),
                                  Q_ARG(QVariant, true));
    }

    qDebug() << "Number of video files to play:" << videoFiles.size();

    return app.exec();
}

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
#include <iostream>
#include <memory>
#include "controller/frameController.h"
#include "controller/videoController.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "ui/videoLoader.h"
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
            QMessageBox::critical(nullptr,
                                  "Error",
                                  QString("Invalid dimensions for video: %1 x %2")
                                      .arg(parser.value(resolutionOption).split("x")[0])
                                      .arg(parser.value(resolutionOption).split("x")[1]));
            return -1;
        }
    }

    QQmlApplicationEngine engine;
    qmlRegisterSingletonType(QUrl("qrc:/Theme.qml"), "Theme", 1, 0, "Theme");
    qmlRegisterType<VideoWindow>("Window", 1, 0, "VideoWindow");

    std::shared_ptr<VideoController> videoController = std::make_shared<VideoController>(nullptr);
    VideoLoader videoLoader(&engine, nullptr, videoController);

    std::vector<VideoFileInfo> videoFiles;
    engine.rootContext()->setContextProperty("videoLoader", &videoLoader);

    engine.rootContext()->setContextProperty("videoController", videoController.get());

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.load(url);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    if (!args.isEmpty() && parser.isSet("r") && parser.isSet("f")) {
        QString filename = args.first();

        qDebug() << "Parsed command-line options. File: " << filename
                 << "Resolution: " << parser.value(resolutionOption) << "Framerate: " << parser.value(framerateOption);

        if (!QFile::exists(filename)) {
            qWarning() << "YUV file does not exist:" << filename;
            QMessageBox::critical(nullptr, "Error", QString("YUV file does not exist: %1").arg(filename));
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
                                  Q_ARG(QVariant, true)); // or false, as appropriate
    }

    qDebug() << "Number of video files to play:" << videoFiles.size();

    return app.exec();
}

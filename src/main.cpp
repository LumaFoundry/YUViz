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
#include "controller/frameController.h"
#include "controller/videoController.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "ui/videoLoader.h"
#include "ui/videoWindow.h"
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

    QCommandLineOption debugOption({"d", "debug"}, "Enable debug output");
    parser.addOption(debugOption);
    parser.process(app);

    if (parser.isSet(debugOption)) {
        qSetMessagePattern("[%{type}] %{message}");
    }

    const QStringList args = parser.positionalArguments();

    if (parser.isSet(debugOption)) {
        qSetMessagePattern("[%{type}] %{message}");
    }

    QQmlApplicationEngine engine;

    SharedViewProperties sharedViewProperties;
    engine.rootContext()->setContextProperty("sharedViewProperties", &sharedViewProperties);

    qmlRegisterSingletonType(QUrl("qrc:/Theme.qml"), "Theme", 1, 0, "Theme");
    qmlRegisterType<VideoWindow>("Window", 1, 0, "VideoWindow");

    std::shared_ptr<VideoController> videoController = std::make_shared<VideoController>(nullptr);
    VideoLoader videoLoader(&engine, nullptr, videoController, &sharedViewProperties);

    std::vector<VideoFileInfo> videoFiles;
    engine.rootContext()->setContextProperty("videoLoader", &videoLoader);

    // videoLoader.loadVideo(filename, width, height, framerate);
    engine.rootContext()->setContextProperty("videoController", videoController.get());

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.load(url);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    qDebug() << "Number of video files to play:" << videoFiles.size();

    return app.exec();
}

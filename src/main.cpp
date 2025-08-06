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
    parser.setApplicationDescription("Visual Inspection Tool\n\n"
                                     "Imports up to two videos from the command line.\n"
                                     "For YUV files, specify parameters separated by colons. Resolution is mandatory.\n"
                                     "Format: path/to/file.yuv:resolution[:framerate][:pixelformat]\n"
                                     "  - Resolution (mandatory): widthxheight (e.g., 1920x1080)\n"
                                     "  - Framerate (optional): A number (e.g., 25). Default: 25.\n"
                                     "  - Pixel Format (optional): 420P, 422P, or 444P. Default: 420P.\n"
                                     "Parameters can be in any order.\n"
                                     "Example: myvideo.yuv:1920x1080:25:444P\n"
                                     "Example: myvideo.yuv:420P:1280x720\n"
                                     "For other formats (e.g., mp4), just provide the path.");
    parser.addVersionOption();
    parser.addHelpOption();
    parser.addPositionalArgument("files", "Video files to open. Up to 2 are supported.", "[file1] [file2]");

    QCommandLineOption debugOption({"d", "debug"}, "Enable debug output");
    parser.addOption(debugOption);

    QCommandLineOption queueSizeOption({"q", "queue-size"}, QLatin1String("Frame queue size"), QLatin1String("size"));
    parser.addOption(queueSizeOption);

    QCommandLineOption softwareOption({"s", "software"},
                                      QLatin1String("Force software decoding (disable hardware acceleration)"));
    parser.addOption(softwareOption);

    parser.process(app);
    const QStringList args = parser.positionalArguments();

    if (parser.isSet(debugOption)) {
        qSetMessagePattern("[%{type}] %{message}");
    }

    if (args.size() > 2) {
        qWarning() << "A maximum of 2 video files can be specified.";
        ErrorReporter::instance().report("A maximum of 2 video files can be specified.", LogLevel::Error);
        return -1;
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

    // Apply global software decoding setting if -s flag is used
    if (parser.isSet(softwareOption)) {
        videoLoader.setGlobalForceSoftwareDecoding(true);
    }

    engine.rootContext()->setContextProperty("videoLoader", &videoLoader);
    engine.rootContext()->setContextProperty("compareController", compareController.get());
    engine.rootContext()->setContextProperty("videoController", videoController.get());

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.load(url);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    if (!args.isEmpty()) {
        QObject* root = engine.rootObjects().first();
        bool forceSoftware = parser.isSet(softwareOption);

        for (const QString& arg : args) {
            QStringList parts = arg.split(':');
            QString filename = parts[0];

            if (!QFile::exists(filename)) {
                QString errorMsg = QString("File does not exist: %1").arg(filename);
                qWarning() << errorMsg;
                ErrorReporter::instance().report(errorMsg, LogLevel::Error);
                return -1;
            }

            // Default values
            int width = 0, height = 0;
            double framerate = 0.0;
            QString yuvFormat = "";

            if (filename.toLower().endsWith(".yuv")) {
                // Set default values for optional parameters
                framerate = 25.0;
                yuvFormat = "420P";

                bool resolutionSet = false;
                bool framerateSet = false;
                bool formatSet = false;

                const int paramCount = parts.size() - 1;
                if (paramCount < 1 || paramCount > 3) {
                    QString errorMsg = QString("Invalid number of parameters for .yuv file '%1'. Expected 1 to 3 "
                                               "(resolution is mandatory), but got %2.")
                                           .arg(filename)
                                           .arg(paramCount);
                    qWarning() << errorMsg;
                    ErrorReporter::instance().report(errorMsg, LogLevel::Error);
                    return -1;
                }

                // Parse parameters in any order
                for (int i = 1; i < parts.size(); ++i) {
                    const QString& part = parts[i];
                    if (part.contains('x', Qt::CaseInsensitive)) { // Resolution
                        if (resolutionSet) {
                            QString errorMsg = QString("Duplicate resolution specified for '%1'.").arg(filename);
                            qWarning() << errorMsg;
                            ErrorReporter::instance().report(errorMsg, LogLevel::Error);
                            return -1;
                        }
                        QStringList resParts = part.split('x');
                        if (resParts.size() != 2) {
                            QString errorMsg =
                                QString("Invalid resolution format '%1'. Expected 'widthxheight'.").arg(part);
                            qWarning() << errorMsg;
                            ErrorReporter::instance().report(errorMsg, LogLevel::Error);
                            return -1;
                        }
                        bool okW, okH;
                        width = resParts[0].toInt(&okW);
                        height = resParts[1].toInt(&okH);
                        if (!okW || !okH || width <= 0 || height <= 0) {
                            QString errorMsg = QString("Invalid resolution value '%1'.").arg(part);
                            qWarning() << errorMsg;
                            ErrorReporter::instance().report(errorMsg, LogLevel::Error);
                            return -1;
                        }
                        resolutionSet = true;

                    } else { // Check if it's numeric (fps) or pixel format
                        bool isNumeric;
                        double fps_candidate = part.toDouble(&isNumeric);

                        if (isNumeric && fps_candidate > 0) { // Framerate
                            if (framerateSet) {
                                QString errorMsg = QString("Duplicate framerate specified for '%1'.").arg(filename);
                                qWarning() << errorMsg;
                                ErrorReporter::instance().report(errorMsg, LogLevel::Error);
                                return -1;
                            }
                            framerate = fps_candidate;
                            framerateSet = true;

                        } else { // Pixel Format
                            if (formatSet) {
                                QString errorMsg = QString("Duplicate pixel format specified for '%1'.").arg(filename);
                                qWarning() << errorMsg;
                                ErrorReporter::instance().report(errorMsg, LogLevel::Error);
                                return -1;
                            }
                            yuvFormat = part.toUpper();
                            formatSet = true;
                        }
                    }
                }

                if (!resolutionSet) {
                    QString errorMsg =
                        QString("Mandatory resolution parameter (e.g., '1920x1080') is missing for .yuv file '%1'.")
                            .arg(filename);
                    qWarning() << errorMsg;
                    ErrorReporter::instance().report(errorMsg, LogLevel::Error);
                    return -1;
                }
            }

            QMetaObject::invokeMethod(root,
                                      "importVideoFromParams",
                                      Qt::QueuedConnection,
                                      Q_ARG(QVariant, filename),
                                      Q_ARG(QVariant, width),
                                      Q_ARG(QVariant, height),
                                      Q_ARG(QVariant, framerate),
                                      Q_ARG(QVariant, yuvFormat),
                                      Q_ARG(QVariant, forceSoftware));
        }
    }

    return app.exec();
}

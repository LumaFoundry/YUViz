#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
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
#include "utils/videoFormatUtils.h"

// These macros are here to prevent editors from complaining
// To change version and name please go to CMakeLists.txt
#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

#ifndef APP_NAME
#define APP_NAME "Visual Inspection Tool"
#endif

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    qDebug() << "Application starting with arguments:" << app.arguments();

    // Set QDebug output to be off by default
    qSetMessagePattern("");

    QCommandLineParser parser;

    QStringList uncompressedFormats;
    for (const VideoFormat& format : VideoFormatUtils::getSupportedFormats()) {
        if (format.type == FormatType::RAW_YUV) {
            uncompressedFormats << format.identifier;
        }
    }

    parser.setApplicationDescription("Visual Inspection Tool\n\n"
                                     "Imports up to two videos from the command line.\n"
                                     "For YUV files, specify parameters separated by colons. Resolution is mandatory.\n"
                                     "Format: path/to/file.yuv:resolution[:framerate][:pixelformat]\n"
                                     "  - Resolution (mandatory): widthxheight (e.g., 1920x1080)\n"
                                     "  - Framerate (optional): A number (e.g., 25). Default: 25.\n"
                                     "  - Pixel Format (optional): One of: " +
                                     uncompressedFormats.join(", ") +
                                     ". Default: 420P.\n"
                                     "Parameters can be in any order.\n"
                                     "Example: myvideo.yuv:1920x1080:25:444P\n"
                                     "Example: myvideo.yuv:420P:1280x720\n"
                                     "For compressed formats (e.g., mp4), just provide the path.");
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
    qmlRegisterSingletonType<VideoFormatUtils>(
        "VideoFormatUtils", 1, 0, "VideoFormatUtils", [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject* {
            Q_UNUSED(engine)
            Q_UNUSED(scriptEngine)
            return new VideoFormatUtils();
        });

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

    // Expose version info to QML
    engine.rootContext()->setContextProperty("APP_NAME", APP_NAME);
    engine.rootContext()->setContextProperty("APP_VERSION", APP_VERSION);
    engine.rootContext()->setContextProperty("BUILD_DATE", __DATE__);

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
            double framerate = 25.0;
            QString pixelFormat = VideoFormatUtils::detectFormatFromExtension(filename);

            if (VideoFormatUtils::getFormatType(pixelFormat) == FormatType::RAW_YUV) {
                // This is a raw YUV file that needs explicit parameters

                bool resolutionSet = false;
                bool framerateSet = false;
                bool formatSet = false;

                // Try to extract resolution and FPS from filename
                QRegularExpression resAndFpsRegex(R"((\d{3,5})x(\d{3,5})[_-](\d{2,3}(?:\.\d{1,2})?))");
                QRegularExpression resOnlyRegex(R"((\d{3,5})x(\d{3,5}))");

                // Extract default values from filename - these can be overridden by command line args
                QRegularExpressionMatch match = resAndFpsRegex.match(filename);
                if (match.hasMatch()) {
                    width = match.captured(1).toInt();
                    height = match.captured(2).toInt();
                    framerate = match.captured(3).toDouble();
                    qDebug() << "Extracted from filename - Resolution:" << width << "x" << height
                             << "FPS:" << framerate;
                } else {
                    match = resOnlyRegex.match(filename);
                    if (match.hasMatch()) {
                        width = match.captured(1).toInt();
                        height = match.captured(2).toInt();
                        qDebug() << "Extracted from filename - Resolution:" << width << "x" << height;
                    }
                }

                // Allow all values to be overridden by command line args
                resolutionSet = false;
                framerateSet = false;
                formatSet = false;

                // Check for command line parameters
                const int paramCount = parts.size() - 1;
                if (paramCount > 3) {
                    QString errorMsg = QString("Too many parameters for .yuv file '%1'. Maximum is 3, but got %2.")
                                           .arg(filename)
                                           .arg(paramCount);
                    qWarning() << errorMsg;
                    ErrorReporter::instance().report(errorMsg, LogLevel::Error);
                    return -1;
                }

                // Parse parameters in any order
                // Parse override parameters in any order
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
                            pixelFormat = part.toUpper();
                            formatSet = true;
                        }
                    }
                }

                // After parsing parameters, check if we have a valid resolution from either source
                if (width <= 0 || height <= 0) {
                    QString errorMsg =
                        QString(
                            "Resolution is required but could not be extracted from filename or parameters for '%1'.\n"
                            "Either include it in the filename (e.g., video_1920x1080.yuv) or specify it as a "
                            "parameter (:1920x1080).")
                            .arg(filename);
                    qWarning() << errorMsg;
                    ErrorReporter::instance().report(errorMsg, LogLevel::Error);
                    return -1;
                }

                qDebug() << "Final parameters for" << filename << "- Resolution:" << width << "x" << height
                         << "FPS:" << framerate << "Format:" << pixelFormat;
            } else {
                // Compressed format - use detected format and set reasonable defaults
                width = 1920; // These will be overridden by decoder
                height = 1080;
                qDebug() << "Compressed format detected:" << pixelFormat << "for file:" << filename;
            }

            QMetaObject::invokeMethod(root,
                                      "importVideoFromParams",
                                      Qt::DirectConnection,
                                      Q_ARG(QVariant, filename),
                                      Q_ARG(QVariant, width),
                                      Q_ARG(QVariant, height),
                                      Q_ARG(QVariant, framerate),
                                      Q_ARG(QVariant, pixelFormat),
                                      Q_ARG(QVariant, forceSoftware));
        }
    }

    return app.exec();
}

#include "videoLoader.h"
#include <QFile>
#include <QQmlContext>
#include <QUrl>
#include "utils/debugManager.h"
#include "utils/videoFormatUtils.h"

VideoLoader::VideoLoader(QQmlApplicationEngine* engine,
                         QObject* parent,
                         std::shared_ptr<VideoController> vcPtr,
                         std::shared_ptr<CompareController> ccPtr,
                         SharedViewProperties* sharedView) :
    QObject(parent),
    m_engine(engine),
    m_vcPtr(vcPtr),
    m_ccPtr(ccPtr),
    m_sharedView(sharedView) {
}

void VideoLoader::loadVideo(
    const QString& filePath, int width, int height, double fps, const QString& pixelFormat, bool forceSoftware) {
    // Apply global software decoding setting if not explicitly overridden
    bool effectiveForceSoftware = forceSoftware || m_globalForceSoftwareDecoding;

    QString path = filePath;
    // Robust normalization for various inputs (URL, local path, Windows-specific forms)
    QUrl inUrl = QUrl::fromUserInput(filePath);
    if (inUrl.isLocalFile()) {
        path = inUrl.toLocalFile();
    }
    // Windows fix: handle paths like "/C:/..."
    if (path.size() > 2 && path[0] == '/' && path[2] == ':') {
        path.remove(0, 1);
    }

    if (!QFile::exists(path)) {
        ErrorReporter::instance().report(QString("File does not exist: %1").arg(path), LogLevel::Error);
        return;
    }

    // Use VideoFormatUtils to convert format string to AVPixelFormat
    AVPixelFormat yuvFormat = VideoFormatUtils::stringToPixelFormat(pixelFormat);

    // Check if the format is valid
    if (!VideoFormatUtils::isValidFormat(pixelFormat)) {
        QString userMessage =
            QString("The pixel format '%1' is not supported.\n\nThe video will not be loaded.").arg(pixelFormat);
        ErrorReporter::instance().report(QString("Failed to load video: %1").arg(userMessage.replace("\n\n", " ")),
                                         LogLevel::Error);

        emit videoLoadFailed("Unsupported Video", userMessage);

        return;
    }

    // Log format type for debugging
    FormatType formatType = VideoFormatUtils::getFormatType(pixelFormat);
    if (formatType == FormatType::COMPRESSED) {
        debug("vl", QString("Loading compressed video format: %1 for file: %2").arg(pixelFormat).arg(path), true);
    } else {
        debug("vl", QString("Loading raw YUV format: %1 for file: %2").arg(pixelFormat).arg(path), true);
    }

    QObject* root = m_engine->rootObjects().first();

    VideoWindow* windowPtr = nullptr;

    QString objectName = QString("videoWindow_%1").arg(index);
    QObject* obj = root->findChild<QObject*>(objectName);
    if (!obj) {
        QObject* qmlBridge = root->findChild<QObject*>("qmlBridge");
        if (!qmlBridge) {
            ErrorReporter::instance().report("Could not find qmlBridge", LogLevel::Error);
            return;
        }
        QVariant returnedValue;
        bool ok = QMetaObject::invokeMethod(
            qmlBridge, "createVideoWindow", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, index));
        if (!ok || !returnedValue.isValid()) {
            ErrorReporter::instance().report(QString("Failed to create VideoWindow with index %1").arg(index),
                                             LogLevel::Error);
            return;
        }
        obj = returnedValue.value<QObject*>();
    }

    windowPtr = qobject_cast<VideoWindow*>(obj);
    if (windowPtr && !windowPtr->property("assigned").toBool()) {
        windowPtr->setProperty("assigned", true);
        windowPtr->setProperty("videoId", index);
        windowPtr->setSharedView(m_sharedView);
    }
    ++index;

    windowPtr->setAspectRatio(width, height);

    VideoFileInfo info;
    info.filename = path;
    info.width = width;
    info.height = height;
    info.framerate = fps;
    info.pixelFormat = yuvFormat;
    info.windowPtr = windowPtr;
    info.forceSoftwareDecoding = effectiveForceSoftware;

    debug("vl", QString("adding video %1").arg(info.filename));
    m_vcPtr->addVideo(info);
}

void VideoLoader::setGlobalForceSoftwareDecoding(bool force) {
    m_globalForceSoftwareDecoding = force;
    if (force) {
        debug("vl", "Global software decoding enabled - all videos will use software decoding");
    }
}

void VideoLoader::setupDiffWindow(int leftId, int rightId) {
    QObject* root = m_engine->rootObjects().first();
    if (!root)
        return;

    // Prefer embedded diff; fall back to popup
    QObject* diffInstance = root->findChild<QObject*>("diffEmbeddedInstance");
    if (!diffInstance)
        diffInstance = root->findChild<QObject*>("diffPopupInstance");

    if (!diffInstance) {
        ErrorReporter::instance().report("Diff popup instance not found", LogLevel::Error);
        return;
    }

    QObject* obj = diffInstance->findChild<QObject*>("diffWindow");
    if (!obj) {
        ErrorReporter::instance().report("Could not find VideoWindow in DiffWindow", LogLevel::Error);
        return;
    }

    auto* diffWindow = qobject_cast<DiffWindow*>(obj);
    if (!diffWindow) {
        ErrorReporter::instance().report("DiffWindow object is not a VideoWindow", LogLevel::Error);
        return;
    }

    diffWindow->setSharedView(m_sharedView);
    diffWindow->setProperty("videoId", -1);
    diffWindow->setProperty("assigned", true);

    // Inject it into CompareController
    m_ccPtr->setDiffWindow(diffWindow);

    // Set up metadata and start diff mode
    m_vcPtr->setDiffMode(true, leftId, rightId);

    debug("vl",
          QString("[setupDiffWindow] wired to %1 leftId=%2 rightId=%3")
              .arg(diffInstance->objectName())
              .arg(leftId)
              .arg(rightId),
          true);
}

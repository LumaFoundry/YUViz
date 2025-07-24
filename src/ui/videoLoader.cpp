#include "videoLoader.h"
#include <QDebug>
#include <QQmlContext>

VideoLoader::VideoLoader(QQmlApplicationEngine* engine, QObject* parent, std::shared_ptr<VideoController> vcPtr) :
    QObject(parent),
    m_engine(engine),
    m_vcPtr(vcPtr) {
}

void VideoLoader::loadVideo(
    const QString& filePath, int width, int height, double fps, const QString& pixelFormat, bool add) {
    QString path;

    if (filePath.startsWith("file://")) {
        path = QUrl(filePath).toLocalFile();
    } else {
        path = filePath;
    }

    if (!QFile::exists(path)) {
        qWarning() << "File does not exist:" << path;
        return;
    }

    AVPixelFormat yuvFormat = AV_PIX_FMT_NONE;

    if (pixelFormat == "420P") {
        yuvFormat = AV_PIX_FMT_YUV420P;
    } else if (pixelFormat == "422P") {
        yuvFormat = AV_PIX_FMT_YUV422P;
    } else if (pixelFormat == "444P") {
        yuvFormat = AV_PIX_FMT_YUV444P;
    }

    QObject* root = m_engine->rootObjects().first();

    VideoWindow* windowPtr = nullptr;

    QString objectName = QString("videoWindow_%1").arg(index);
    QObject* obj = root->findChild<QObject*>(objectName);
    if (!obj) {
        QObject* qmlBridge = root->findChild<QObject*>("qmlBridge");
        if (!qmlBridge) {
            qWarning() << "Could not find qmlBridge";
            return;
        }
        QVariant returnedValue;
        bool ok = QMetaObject::invokeMethod(
            qmlBridge, "createVideoWindow", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, index));
        if (!ok || !returnedValue.isValid()) {
            qWarning() << "Failed to create VideoWindow with index" << index;
            return;
        }
        obj = returnedValue.value<QObject*>();
    }
    windowPtr = qobject_cast<VideoWindow*>(obj);
    if (windowPtr && !windowPtr->property("assigned").toBool()) {
        windowPtr->setProperty("assigned", true);
        windowPtr->setProperty("videoId", index);
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

    if (add) {
        qDebug() << "adding video" << info.filename;
        m_vcPtr->addVideo(info);
    } else {
        qDebug() << "resetting video" << info.filename;
        // m_vcPtr->resetVideo(info);
    }
}

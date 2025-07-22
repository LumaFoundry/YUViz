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
    auto* windowPtr = root->findChild<VideoWindow*>("videoWindow_0");
    if (!windowPtr) {
        qWarning() << "Could not find VideoWindow with id 'videoWindow_0'";
        return;
    }

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

    qDebug() << "starting vc";
    m_vcPtr->start();
}

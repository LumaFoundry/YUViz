#include "videoLoader.h"
#include <QDebug>
#include <QQmlContext>

VideoLoader::VideoLoader(QQmlApplicationEngine* engine, QObject* parent, std::shared_ptr<VideoController> vcPtr) :
    QObject(parent),
    m_engine(engine),
    m_vcPtr(vcPtr) {
}

void VideoLoader::loadVideo(const QString& filePath, int width, int height, double fps) {
    QString path = QUrl(filePath).toLocalFile();
    // QString path = filePath;

    if (!QFile::exists(path)) {
        qWarning() << "File does not exist:" << path;
        return;
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
    info.windowPtr = windowPtr;

    qDebug() << "adding video" << info.filename;
    m_vcPtr->addVideo(info);
    qDebug() << "starting vc";
    m_vcPtr->start();
}

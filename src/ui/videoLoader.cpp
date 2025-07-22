#include "videoLoader.h"
#include <QDebug>
#include <QQmlContext>

VideoLoader::VideoLoader(QQmlApplicationEngine* engine, QObject* parent, std::shared_ptr<VideoController> vcPtr) :
    QObject(parent),
    m_engine(engine),
    m_vcPtr(vcPtr) {
}

void VideoLoader::loadVideo(const QString& filePath, int width, int height, double fps, bool add) {
    QString path = QUrl(filePath).toLocalFile();
    // QString path = filePath;

    if (!QFile::exists(path)) {
        qWarning() << "File does not exist:" << path;
        return;
    }

    QObject* root = m_engine->rootObjects().first();
    int index = 0;
    VideoWindow* windowPtr = nullptr;
    while (true) {
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
            break;
        }
        ++index;
    }
    windowPtr->setAspectRatio(width, height);

    VideoFileInfo info;
    info.filename = path;
    info.width = width;
    info.height = height;
    info.framerate = fps;
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

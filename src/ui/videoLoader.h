#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include "controller/videoController.h"
#include "ui/videoWindow.h"
#include "utils/sharedViewProperties.h" // Add this
#include "utils/videoFileInfo.h"

class VideoLoader : public QObject {
    Q_OBJECT

  public:
    explicit VideoLoader(QQmlApplicationEngine* engine,
                         QObject* parent = nullptr,
                         std::shared_ptr<VideoController> vcPtr = nullptr,
                         SharedViewProperties* sharedView = nullptr);

    Q_INVOKABLE void loadVideo(
        const QString& filePath, int width, int height, double fps, const QString& pixelFormat, bool add);

  private:
    QQmlApplicationEngine* m_engine;
    std::shared_ptr<VideoController> m_vcPtr;
    int index = 0;
    SharedViewProperties* m_sharedView;
};

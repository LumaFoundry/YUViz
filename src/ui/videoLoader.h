#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include "controller/compareController.h"
#include "controller/videoController.h"
#include "ui/diffWindow.h"
#include "ui/videoWindow.h"
#include "utils/sharedViewProperties.h"
#include "utils/videoFileInfo.h"

class VideoLoader : public QObject {
    Q_OBJECT

  public:
    explicit VideoLoader(QQmlApplicationEngine* engine,
                         QObject* parent = nullptr,
                         std::shared_ptr<VideoController> vcPtr = nullptr,
                         std::shared_ptr<CompareController> ccPtr = nullptr,
                         SharedViewProperties* sharedView = nullptr);

    Q_INVOKABLE void loadVideo(const QString& filePath,
                               int width,
                               int height,
                               double fps,
                               const QString& pixelFormat,
                               bool forceSoftware = false);

    Q_INVOKABLE void setupDiffWindow(int leftId, int rightId);

    void setGlobalForceSoftwareDecoding(bool force);

  private:
    QQmlApplicationEngine* m_engine;
    std::shared_ptr<VideoController> m_vcPtr;
    std::shared_ptr<CompareController> m_ccPtr;
    int index = 0;
    SharedViewProperties* m_sharedView;
    bool m_globalForceSoftwareDecoding = false;
};

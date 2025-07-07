#pragma once

#include <QThread>
#include <QElapsedTimer>
#include <QtConcurrent>
#include <QVariant>
#include <map>

#include "frameController.h"
#include "frames/frameQueue.h"
#include "frames/frameMeta.h"
#include "frames/frameData.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "utils/errorReporter.h"
#include "controller/timer.h"
#include "ui/videoWindow.h"
#include "utils/videoFileInfo.h"

class VideoController : public QObject {
    Q_OBJECT

public:
    VideoController(QObject *parent, 
                    std::vector<VideoFileInfo> videoFiles = {});
    ~VideoController();
    void start();

public slots:
    void onReady(int index);
    void onFCEndOfVideo(int index);
    void onTick(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs);

signals:
    void playTimer();
    void stopTimer();
    void pauseTimer();
    void tickFC(int64_t pts);

private:
    std::vector<std::unique_ptr<FrameController>> m_frameControllers;

    std::shared_ptr<Timer> m_timer;

    QThread m_timerThread;

    // Ensure all FC have uploaded initial frame before starting timer
    int m_readyCount = 0;

    int m_endCount = 0;

};


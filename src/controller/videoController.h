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
#include "controller/playBackWorker.h"

struct VideoFileInfo {
    QString filename;
    int width;
    int height;
    double framerate;
    QRhi::Implementation graphicsApi;
};

class VideoController : public QObject {
    Q_OBJECT

public:
    VideoController(QObject *parent, 
                    std::shared_ptr<PlaybackWorker> playbackWorker,
                    std::vector<VideoFileInfo> videoFiles = {});
    ~VideoController();

    void addFrameController(FrameController* controller);
    std::vector<FrameController*> getFrameControllers();

public slots:
    void uploadReady(bool success);
    void synchroniseFC(int64_t delta, int index);
    void onFCEndOfVideo(int index);
    void togglePlayPause();

signals:
    void get_next_tick(int64_t delta);
    void startPlayback();
    void stopPlayback();
    void pausePlayback();
    void resumePlayback();

private:
    std::shared_ptr<PlaybackWorker> m_playbackWorker;
    std::vector<std::unique_ptr<FrameController>> m_frameControllers;

    QThread m_timerThread;

    std::vector<std::shared_ptr<VideoWindow>> m_windowPtrs;
    // Ensure all FC have uploaded initial frame before starting timer
    int m_readyCount = 0;

};


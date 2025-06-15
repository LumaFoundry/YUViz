#pragma once

#include <QThread>
#include <QElapsedTimer>
#include <QtConcurrent>
#include "FrameQueue.h"
#include "FrameMeta.h"
#include "FrameData.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "utils/errorReporter.h"

// One controller per FrameQueue

class FrameController : public QObject{

    Q_OBJECT

public:

    FrameController(QObject *parent, VideoDecoder* decoder, VideoRenderer* renderer);
    ~FrameController();

    // Start decoder and renderer threads
    void start();

    // TODO: Frame read control
    // TODO: Pass frameData ptr to renderer
    // TODO: Playback control
    // TODO: QElapsedTimer for frame timing
    void play(bool resumed=false);
    void pause();
    void resume();
    void reverse(int dir);
    void changeSpeed(float speed);

public slots:
    // Receive signals from decoder and renderer
    void onTimerTick();
    void onFrameDecoded(bool success);
    void onFrameUploaded(bool success);
    void onRenderError();
   

signals:
    void requestDecode(FrameData* frame);
    void requestUpload(FrameData* frame);
    void requestRender();

private:

    // FrameQueue to manage frames
    FrameQueue m_frameQueue;

    // YUVReader to read frames from video file
    std::unique_ptr<VideoDecoder> m_Decoder = nullptr;

    // VideoRenderer to render frames
    std::unique_ptr<VideoRenderer> m_Renderer = nullptr;

    // Thread for reading / writing frames object
    QThread m_renderThread;
    QThread m_decodeThread;

    // Timer for synchronization
    QElapsedTimer m_timer;

    bool m_playing = false;

    // Playback control variables
    double m_speed = 1.0;
    int m_direction = 1; // 1 = forward, 0 = pause, -1 = reverse

    int64_t m_nextWakeMs = 0;
    int64_t m_lastPTS = 0;
    int64_t m_pausedAt = 0;

};

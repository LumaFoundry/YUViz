#pragma once

#include <QThread>
#include <QElapsedTimer>
#include <QtConcurrent>
#include "frames/FrameQueue.h"
#include "frames/FrameMeta.h"
#include "frames/FrameData.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "utils/errorReporter.h"
#include "controller/playBackWorker.h"

// One controller per FrameQueue

class FrameController : public QObject{
    Q_OBJECT

public:

    FrameController(QObject *parent, VideoDecoder* decoder, VideoRenderer* renderer);
    ~FrameController();

    // Start decoder, renderer and timer threads
    void start();

    // Interface method for playback control for UI
    void play();
    void pause();
    void resume();
    void step();
    void stop();

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
    void requestNextSleep(int64_t deltaMs);

    // For interface control
    void requestPlaybackStart();
    void requestPlaybackPause();
    void requestPlaybackResume();
    void requestPlaybackStep();
    void requestPlaybackStop();

private:

    // FrameQueue to manage frames
    FrameQueue m_frameQueue;

    // YUVReader to read frames from video file
    std::unique_ptr<VideoDecoder> m_Decoder = nullptr;

    // VideoRenderer to render frames
    std::unique_ptr<VideoRenderer> m_Renderer = nullptr;

    // Playback worker to control playback
    std::unique_ptr<PlaybackWorker> m_playbackWorker = nullptr;

    // Thread for reading / writing frames object
    QThread m_renderThread;
    QThread m_decodeThread;
    QThread m_timerThread;

    // Last PTS for calculating delta time
    int64_t m_lastPTS = 0;

    // Playback control variables
    double m_speed = 1.0;
    int m_direction = 1; // 1 = forward, 0 = pause, -1 = reverse

};

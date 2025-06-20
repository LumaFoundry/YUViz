#pragma once

#include <QThread>
#include <QElapsedTimer>
#include <QtConcurrent>
#include "frames/frameQueue.h"
#include "frames/frameMeta.h"
#include "frames/frameData.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "utils/errorReporter.h"
#include "controller/playBackWorker.h"

// One controller per FrameQueue

class FrameController : public QObject{
    Q_OBJECT

public:

    FrameController(QObject *parent, VideoDecoder* decoder, VideoRenderer* renderer, std::shared_ptr<PlaybackWorker> playbackWorker, int index);
    ~FrameController();

    // Start decoder, renderer and timer threads
    void start();

    int m_index; // Index of current FC, for VC orchestration

    // For VC to receive renderer signal
    VideoRenderer* getRenderer() const { return m_Renderer.get(); }
    

public slots:
    // Receive signals from decoder and renderer
    void onTimerTick();
    void onFrameDecoded(bool success);
    void onFrameUploaded(bool success);
    void onFrameRendered(bool success);
    void onRenderError();
   
signals:
    void requestDecode(FrameData* frame);
    void requestUpload(FrameData* frame);
    void requestRender();
    void currentDelta(int64_t delta, int index);

private:

    // YUVReader to read frames from video file
    std::unique_ptr<VideoDecoder> m_Decoder = nullptr;

    // VideoRenderer to render frames
    std::unique_ptr<VideoRenderer> m_Renderer = nullptr;

    // PlaybackWorker to manage timer ticks
    std::shared_ptr<PlaybackWorker> m_PlaybackWorker = nullptr;

    // FrameQueue to manage frames
    FrameQueue m_frameQueue;

    // Thread for reading / writing frames object
    QThread m_renderThread;
    QThread m_decodeThread;

    // Last PTS of the frame rendered
    int64_t m_lastPTS = -1; 

};

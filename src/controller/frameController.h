#pragma once

#include <QThread>
#include <QElapsedTimer>
#include <QtConcurrent>
#include <utility>
#include "frames/frameQueue.h"
#include "frames/frameMeta.h"
#include "frames/frameData.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "utils/errorReporter.h"
#include "controller/playBackWorker.h"
#include "ui/videoWindow.h"

// One controller per FrameQueue

class FrameController : public QObject{
    Q_OBJECT

public:

    FrameController(QObject *parent, 
                    std::unique_ptr<VideoDecoder> decoder, 
                    std::shared_ptr<PlaybackWorker> playbackWorker,
                    VideoWindow* window, 
                    int index);
                
    ~FrameController();

    // Start decoder, renderer and timer threads
    void start();

    int m_index; // Index of current FC, for VC orchestration

public slots:
    // Receive signals from decoder and renderer
    void onTimerTick();
    void onFrameDecoded(bool success);
    void onFrameUploaded(bool success);
    void onFrameRendered(bool success);
    void onPrefillCompleted(bool success);
    void onRenderError();
   
signals:
    void requestDecode(FrameData* frame);
    void requestUpload(FrameData* frame);
    void requestRender();
    void currentDelta(int64_t delta, int index);
    void endOfVideo(int index);

private:

    // YUVReader to read frames from video file
    std::unique_ptr<VideoDecoder> m_Decoder = nullptr;

    // PlaybackWorker to manage timer ticks
    std::shared_ptr<PlaybackWorker> m_PlaybackWorker = nullptr;

    // For display
    VideoWindow* m_window = nullptr;

    // FrameQueue to manage frames
    FrameQueue m_frameQueue;

    // Thread for reading / writing frames object
    QThread m_renderThread;
    QThread m_decodeThread;

    // Last PTS of the frame rendered
    int64_t m_lastPTS = -1; 

    // Prefill count for preloading frames
    int m_prefillDecodedCount = 0; 

};

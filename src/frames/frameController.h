#pragma once

#include "FrameQueue.h"
#include "FrameMeta.h"
#include "FrameData.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include <QThread>

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
    void play();
    void pause();
    void reverse(int dir);
    void changeSpeed(float speed);

public slots:
    // Receive signals from decoder and renderer
    void onFrameDecoded();
    void onFrameRendered();

signals:
    void requestRender(FrameData* frame);
    void requestDecode(FrameData* frame);

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

};

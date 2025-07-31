#pragma once

#include <QElapsedTimer>
#include <QThread>
#include <QtConcurrent>
#include <utility>
#include "decoder/videoDecoder.h"
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "frames/frameQueue.h"
#include "ui/videoWindow.h"
#include "utils/errorReporter.h"
#include "utils/videoFileInfo.h"

extern "C" {
#include <libavutil/rational.h>
}

// One controller per FrameQueue

class FrameController : public QObject {
    Q_OBJECT

  public:
    FrameController(QObject* parent, VideoFileInfo videoFileInfo, int index);

    ~FrameController();

    // Start decoder, renderer and timer threads
    void start();
    void onTimerTick(int64_t pts, int direction);
    void onTimerStep(int64_t pts, int direction);

    void onSeek(int64_t pts);

    AVRational getTimeBase();
    std::shared_ptr<FrameMeta> getFrameMeta() const { return m_frameMeta; }
    std::shared_ptr<FrameQueue> getFrameQueue() const { return m_frameQueue; }

    int m_index; // Index of current FC, for VC orchestration

    int totalFrames();
    int64_t getDuration();

  public slots:
    // Receive signals from decoder and renderer
    void onFrameDecoded(bool success);
    void onFrameUploaded();
    void onFrameRendered();
    void onRenderError();
    void onFrameSeeked(int64_t pts);

  signals:
    void ready(int index);
    void requestDecode(int numFrames, int direction);
    void requestUpload(FrameData* frame, int index);
    void requestRender(int index);
    void endOfVideo(bool end, int index);
    void requestSeek(int64_t pts, int loadCount);
    void smartSeekUpdate(int64_t pts);

  private:
    // YUVReader to read frames from video file
    std::unique_ptr<VideoDecoder> m_Decoder = nullptr;

    // For display
    VideoWindow* m_window = nullptr;

    // FrameQueue to manage frames
    std::shared_ptr<FrameQueue> m_frameQueue;

    std::shared_ptr<FrameMeta> m_frameMeta;

    // Thread for writing frames object
    QThread m_decodeThread;

    // Last PTS of the frame rendered
    int64_t m_lastPTS = -1;

    // Prefill flag for preloading frames
    bool m_prefill = false;

    int64_t m_stepping = -1;
    int m_direction = 1; // 1 for forward, -1 for backward

    bool m_endOfVideo = false;

    int64_t m_seeking = -1;
};

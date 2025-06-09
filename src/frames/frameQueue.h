#pragma once

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QList>
#include <memory>
#include "frameData.h"
#include "../readers/frameReader.h"

class FrameReader;

class FrameQueue : public QThread {
    Q_OBJECT

public:

    FrameQueue();
    ~FrameQueue() override;

    void run() override;
    void wake();    // wake the thread when a frame was consumed

    std::unique_ptr<FrameData> allocateFrame();
    void releaseFrame(std::unique_ptr<FrameData> frame);

    void setReader(FrameReader* r)  { reader = r; }

    std::unique_ptr<FrameData> getNextFrame(int speed, int direction);

private:
    int calNextFrameIndex(bool future);
    void addFrames(bool future);

    FrameReader* reader = nullptr;

    // Recently displayed frames (for seeking/backstep)
    QList<std::unique_ptr<FrameData>> pastFrames;

    // Buffered frames ready for upcoming display
    QList<std::unique_ptr<FrameData>> futureFrames;

    // Pool of reusable FrameData objects (memory recycling)
    QList<std::unique_ptr<FrameData>> usedFrames;

    // Mutex for thread-safe access to frame lists
    QMutex listMutex;

    // Currently displayed frame (for rendering)
    std::unique_ptr<FrameData> displayFrame;
    int displayFrameIndex = 0;
    
    // Playback control variables
    int direction = 0;  // 0: stopped, 1: forward, -1: backward
    int speed = 0;

    // Condition variable to signal when a frame is consumed
    QWaitCondition frameConsumed;
    // Mutex to protect frame access
    QMutex frameMutex;

    bool m_doReading = true;
};

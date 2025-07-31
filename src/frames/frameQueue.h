#pragma once

#include <QDebug>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <cstdint>
#include "frameData.h"
#include "frameMeta.h"

class FrameQueue {
  public:
    // Takes in FrameMeta to initialize the queue
    FrameQueue(std::shared_ptr<FrameMeta> meta, int queueSize = 50);
    ~FrameQueue();

    // Getter for metaData
    std::shared_ptr<FrameMeta> metaPtr() const { return m_metaPtr; };

    // Getter the current frame data pointer for renderer
    // Should be thread-safe - block until there is frame to read
    FrameData* getHeadFrame(int64_t pts);

    // Get the next frame to be loaded for decoder
    // Should be thread-safe - block until there is space to write
    FrameData* getTailFrame(int64_t pts);

    void updateTail(int64_t pts);

    int isForwardSeek(int64_t pts);
    bool isLargeGap(int64_t pts);

    const int getSize() const { return m_queueSize; }

    int getEmpty(int direction);

  private:
    // Size of the queue
    const int m_queueSize;

    // Points to current frame
    // access by using
    // size_t headVal = head.load(std::memory_order_acquire);
    std::atomic<int64_t> head = 0;

    // Points to future un-loaded frame
    // access by using
    // size_t tailVal = tail.load(std::memory_order_acquire);
    std::atomic<int64_t> tail = 0;

    // Frame metadata
    std::shared_ptr<FrameMeta> m_metaPtr;

    // Shared buffer for the raw YUV data
    std::shared_ptr<std::vector<uint8_t>> m_bufferPtr;

    // Frame data queue
    std::vector<FrameData> m_queue;
};
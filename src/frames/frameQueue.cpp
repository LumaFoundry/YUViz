#include "frameQueue.h"

FrameQueue::FrameQueue(std::shared_ptr<FrameMeta> meta) :
    m_metaPtr(meta) {
    int ySize = m_metaPtr->ySize();
    int uvSize = m_metaPtr->uvSize();
    size_t frameSize = ySize + uvSize * 2;
    size_t bufferSize = frameSize * queueSize;

    m_bufferPtr = std::make_shared<std::vector<uint8_t>>();
    m_bufferPtr->reserve(bufferSize);

    // Allocate frame data queue
    m_queue.reserve(queueSize);
    for (int i = 0; i < queueSize; ++i) {
        m_queue.emplace_back(ySize, uvSize, m_bufferPtr, i * frameSize);
    }
}

FrameQueue::~FrameQueue() = default;

// Prevent renderer / decoder from modifying pointers when the other is accessing

int FrameQueue::getEmpty(int direction) {
    int64_t tailVal = tail.load(std::memory_order_acquire);
    int64_t headVal = head.load(std::memory_order_acquire);
    // qDebug() << "Queue:: tail: " << tailVal << "head: " << headVal;

    int empty = 0;

    if (direction == 1) {
        empty = (headVal + queueSize / 2) - tailVal;
    } else {
        empty = (tailVal + queueSize / 2) - headVal;
    }

    // qDebug() << "Queue:: empty frames: " << empty;

    if (empty < 0) {
        empty = 0;
    }

    return empty;
}

// IMPORTANT: Must not call decoder when seeking / stepping
FrameData* FrameQueue::getHeadFrame(int64_t pts) {
    // qDebug() << "Queue:: Tail: " << (tail.load(std::memory_order_acquire));
    FrameData* target = &m_queue[pts % queueSize];

    // Check if target is loaded in queue
    if (target->pts() == pts) {
        // Update head if frame is in queue
        head.store(pts, std::memory_order_release);
        return target;
    }

    return nullptr;
}

FrameData* FrameQueue::getTailFrame(int64_t pts) {
    // qDebug() << "Queue:: Tail index: " << (pts % queueSize);
    return &m_queue[pts % queueSize];
}

// IMPORTANT: Needs to be called after done decoding
void FrameQueue::updateTail(int64_t pts) {
    // qDebug() << "Queue:: updateTail called with pts: " << pts;
    if (pts > 0) {
        tail.store(pts, std::memory_order_release);
    }
}

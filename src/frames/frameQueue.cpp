#include "frameQueue.h"

FrameQueue::FrameQueue(std::shared_ptr<FrameMeta> meta, int queueSize) :
    m_metaPtr(meta),
    m_queueSize(queueSize) {
    int ySize = m_metaPtr->ySize();
    int uvSize = m_metaPtr->uvSize();
    size_t frameSize = ySize + uvSize * 2;
    size_t bufferSize = frameSize * m_queueSize;

    m_bufferPtr = std::make_shared<std::vector<uint8_t>>();
    m_bufferPtr->reserve(bufferSize);

    // Allocate frame data queue
    m_queue.reserve(m_queueSize);
    for (int i = 0; i < m_queueSize; ++i) {
        m_queue.emplace_back(ySize, uvSize, m_bufferPtr, i * frameSize);
    }
}

FrameQueue::~FrameQueue() = default;

// Prevent renderer / decoder from modifying pointers when the other is accessing

int FrameQueue::getEmpty(int direction) {
    int64_t tailVal = tail.load(std::memory_order_acquire);
    int64_t headVal = head.load(std::memory_order_acquire);

    int empty = 0;

    if (direction == 1) {
        empty = (headVal + m_queueSize / 2) - tailVal;
    } else {
        empty = (tailVal + m_queueSize / 2) - headVal;
    }

    qDebug() << "Queue:: tail: " << tailVal << "head: " << headVal << "empty: " << empty;

    if (empty < 0) {
        empty = 0;
    }

    return empty;
}

// IMPORTANT: Must not call decoder when seeking / stepping
FrameData* FrameQueue::getHeadFrame(int64_t pts) {
    // qDebug() << "Queue:: Tail: " << (tail.load(std::memory_order_acquire));
    FrameData* target = &m_queue[pts % m_queueSize];

    // Check if target is loaded in queue
    if (target->pts() == pts) {
        // Update head if frame is in queue
        head.store(pts, std::memory_order_release);
        return target;
    }

    return nullptr;
}

FrameData* FrameQueue::getTailFrame(int64_t pts) {
    // qDebug() << "Queue:: Tail index: " << (pts % m_queueSize);
    return &m_queue[pts % m_queueSize];
}

// IMPORTANT: Needs to be called after done decoding
void FrameQueue::updateTail(int64_t pts) {
    // qDebug() << "Queue:: updateTail called with pts: " << pts;
    if (pts >= 0) {
        tail.store(pts, std::memory_order_release);
    }
}

void FrameQueue::realignPointers(int64_t pts) {
    head.store(pts, std::memory_order_release);
    tail.store(pts, std::memory_order_release);
}

bool FrameQueue::isStale(int64_t pts) {
    int64_t tailVal = tail.load(std::memory_order_acquire);
    int size = m_queueSize;
    // Window is [tailVal - size + 1, tailVal]
    return !((pts >= tailVal - size + 1) && (pts <= tailVal));
}
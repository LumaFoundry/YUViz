#include "frameQueue.h"

FrameQueue::FrameQueue(FrameMeta meta)
{
    m_metaPtr = std::make_shared<FrameMeta>(meta);
    
    int ySize = m_metaPtr->ySize();
    int uvSize = m_metaPtr->uvSize();
    size_t frameSize = ySize + uvSize * 2;
    size_t bufferSize = frameSize * queueSize;

    m_bufferPtr = std::make_shared<std::vector<uint8_t>>(bufferSize);

    // Allocate frame data queue
    m_queue.reserve(queueSize);
    for (int i = 0; i < queueSize; ++i) {
        m_queue.emplace_back(ySize, uvSize, m_bufferPtr, i * frameSize);
    }
}

FrameQueue::~FrameQueue() = default;

// Prevent renderer / decoder from modifying pointers when the other is accessing

int FrameQueue::getEmpty(){
    size_t tailVal = tail.load(std::memory_order_acquire);
    size_t headVal = head.load(std::memory_order_acquire);

    return (headVal + queueSize / 2 - tailVal + queueSize) % queueSize;
}

// IMPORTANT: Must not call decoder when seeking / stepping
FrameData* FrameQueue::getHeadFrame(int64_t pts) {

    FrameData* target = &m_queue[pts % queueSize];

    // Check if target is loaded in queue
    if (target->pts() == pts) {
        // Update head if frame is in queue
        head.store(pts, std::memory_order_release);
        return target;
    }

    return nullptr;
}


FrameData* FrameQueue::getTailFrame(int64_t pts){
    size_t tailVal = tail.load(std::memory_order_acquire);
    // qDebug() << "Queue:: Tail index: " << (tailVal % queueSize);
    return &m_queue[tailVal % queueSize];
}


// IMPORTANT: Needs to be called after done decoding
void FrameQueue::updateTail(int64_t pts){
    tail.store(pts, std::memory_order_release);
}


void FrameQueue::clear(){
    head.store(0, std::memory_order_release);
    tail.store(0, std::memory_order_release);
    
    // We don't need to actually clear the queue as it just gets overwritten
    // We could in theory set all pts to -1, but that's unnecessary
}
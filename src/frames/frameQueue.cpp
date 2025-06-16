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

bool FrameQueue::isEmpty() const {
    return head >= tail;
}

bool FrameQueue::isFull() const {
    return tail - head > queueSize / 2;
}

// Prevent renderer / decoder from modifying pointers when the other is accessing
FrameData* FrameQueue::getHeadFrame(){
    QMutexLocker locker(&m_mutex);

    // Wait until there is frame to read
    while (isEmpty()) {
        m_canRead.wait(&m_mutex);
    }
    return &m_queue[head % queueSize];
}

// IMPORTANT: Needs to be called after each frame is rendered
void FrameQueue::incrementHead(){
    QMutexLocker locker(&m_mutex);
    head += 1;
    // Notify decoder it is okay to write
    m_canWrite.wakeOne();
}


FrameData* FrameQueue::getTailFrame(){
    QMutexLocker locker(&m_mutex);

    // Wait until there is space to write
    while (isFull()){
        m_canWrite.wait(&m_mutex);
    }
    return &m_queue[tail % queueSize];
}

// IMPORTANT: Needs to be called after each frame is loaded
void FrameQueue::incrementTail(){
    QMutexLocker locker(&m_mutex);
    tail += 1;
    // Notify renderer it is okay to read
    m_canRead.wakeOne();
}




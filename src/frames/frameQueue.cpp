#include "frameQueue.h"

FrameQueue::FrameQueue(FrameMeta* meta) : m_meta(meta) {

    size_t frameSize = meta->ySize() + meta->uvSize() * 2;

    size_t bufferSize = frameSize * queueSize;

    m_memoryPool = std::make_shared<std::vector<uint8_t>>(bufferSize);

    // Allocate frame data queue
    m_queue.resize(queueSize);
    for (int i = 0; i < queueSize; ++i) {
        m_queue[i] = FrameData(meta->yWidth() * meta->yHeight(),
                                meta->uvWidth() * meta->uvHeight(),
                                0, m_memoryPool, i * frameSize);
    }

}

bool FrameQueue::isEmpty() const {
    return head >= tail;
}

bool FrameQueue::isFull() const {
    return (tail + 1) % queueSize >= head;
}

// Prevent renderer / decoder from modifying pointers when the other is accessing
FrameData* FrameQueue::getHeadFrame(){
    QMutexLocker locker(&m_mutex);

    // Wait until there is frame to read
    while (isEmpty()) {
        m_canRead.wait(&m_mutex);
    }
    return &m_queue[head];
}

// IMPORTANT: Needs to be called after each frame is rendered
void FrameQueue::incrementHead(){
    QMutexLocker locker(&m_mutex);
    head = (head + 1) % queueSize;
    // Notify decoder it is okay to write
    m_canWrite.wakeOne();
}


FrameData* FrameQueue::getTailFrame(){
    QMutexLocker locker(&m_mutex);

    // Wait until there is space to write
    while (isFull()){
        m_canWrite.wait(&m_mutex);
    }
    return &m_queue[tail];
}

// IMPORTANT: Needs to be called after each frame is loaded
void FrameQueue::incrementTail(){
    QMutexLocker locker(&m_mutex);
    tail = (tail + 1) % queueSize;
    // Notify renderer it is okay to read
    m_canRead.wakeOne();
}




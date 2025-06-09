#include "frameQueue.h"

#define MAX_READS_PER_CALL 10
#define MAX_LEN 50

FrameQueue::FrameQueue() : QThread() {
    // Initialize the thread
    m_doReading = true;
    displayFrame = nullptr;
    displayFrameIndex = 0;
    direction = 0;
    speed = 0;
}

FrameQueue::~FrameQueue() {
    // Stop the thread
    m_doReading = false;
    wake();
    wait();

    // Clear frames
    displayFrame.reset();
    pastFrames.clear();
    futureFrames.clear();
    usedFrames.clear();
}

int FrameQueue::calNextFrameIndex(bool future) {

    int lastFrameIndex = displayFrameIndex;
    int nextFrameIndex;

    // If reader cannot random access, return next frame in queue
    if (! reader->randomAccess)
        return 0;

    // If seek is possible, get index of last frame (according to direction)
    if (future){
        if (futureFrames.size())
            lastFrameIndex = futureFrames.last()->frameIndex;
    }else {
        if (pastFrames.size())
            lastFrameIndex = pastFrames.last()->frameIndex;
    }

    // Ensure offset not affected when video is paused
    int offset = (direction == 0) ? speed : speed * direction;
    nextFrameIndex = lastFrameIndex + offset;

    return nextFrameIndex;
}

void FrameQueue::addFrames(bool future) {

    QList<std::unique_ptr<FrameData>>& queue = future ? futureFrames : pastFrames;

    int stop = MAX_READS_PER_CALL;

    QMutexLocker locker(&listMutex);
    int numFrames = queue.size();
    locker.unlock();

    // Read frames until queue is full or reached max reads per call
    while (stop-- && numFrames < MAX_LEN){

        int nextFrameIndex = calNextFrameIndex(future);
        std::unique_ptr<FrameData> frame;
        reader->pullFrame(nextFrameIndex, frame);

        locker.relock();
        queue.append(std::move(frame));
        numFrames = queue.size();
        locker.unlock();
    }

}

std::unique_ptr<FrameData> getNextFrame(int speed, int direction){
    
}






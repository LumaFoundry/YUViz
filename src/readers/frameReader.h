#pragma once

#include "../frames/frameData.h"
#include "../frames/frameQueue.h"

class FrameReader {
public:
    explicit FrameReader(FrameQueue& frameQ) : frameQueue(frameQ){}
    virtual ~FrameReader() = default;

    FrameReader(const FrameReader&) = delete;
    FrameReader& operator=(const FrameReader&) = delete;

    virtual void pullFrame(int frameIndex, std::unique_ptr<FrameData>& dst) = 0;

    bool randomAccess = false;

protected:
    FrameQueue& frameQueue;

};

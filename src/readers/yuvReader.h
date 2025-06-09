#pragma once

#include "frameReader.h"
#include "../frames/frameData.h"
#include "../frames/frameQueue.h"

class YUVReader : public FrameReader {
public:
    explicit YUVReader(FrameQueue& frameQ) : FrameReader(frameQ) {}

    void pullFrame(int frameIndex, std::unique_ptr<FrameData>& dst) override {}

};
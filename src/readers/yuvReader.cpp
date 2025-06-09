#include "yuvReader.h"
#include "../frames/frameQueue.h"

YUVReader::YUVReader(FrameQueue& frameQ) : FrameReader(frameQ) {
    randomAccess = true;
}

void YUVReader::pullFrame(int frameIndex, std::unique_ptr<FrameData>& dst) {}

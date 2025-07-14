#include "frameData.h"
#include <cassert>
#include <memory>

FrameData::FrameData(int ySize, int uvSize, std::shared_ptr<std::vector<uint8_t>> bufferPtr, size_t bufferOffset) :
    m_bufferPtr(bufferPtr),
    m_bufferOffset(bufferOffset) {
    m_planeOffset[0] = 0;
    m_planeOffset[1] = ySize;
    m_planeOffset[2] = ySize + uvSize;
}

FrameData::~FrameData() = default;

uint8_t* FrameData::yPtr() const {
    if (!m_bufferPtr)
        return nullptr;
    return m_bufferPtr->data() + m_bufferOffset + m_planeOffset[0];
}

uint8_t* FrameData::uPtr() const {
    if (!m_bufferPtr)
        return nullptr;
    return m_bufferPtr->data() + m_bufferOffset + m_planeOffset[1];
}

uint8_t* FrameData::vPtr() const {
    if (!m_bufferPtr)
        return nullptr;
    return m_bufferPtr->data() + m_bufferOffset + m_planeOffset[2];
}

int64_t FrameData::pts() const {
    return m_pts;
}

void FrameData::setPts(int64_t pts) {
    m_pts = pts;
}

bool FrameData::isEndFrame() const {
    return m_isEndFrame;
}

void FrameData::setEndFrame(bool isEndFrame) {
    m_isEndFrame = isEndFrame;
}

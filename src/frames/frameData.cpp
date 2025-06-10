#include "frameData.h"
#include <cassert>
#include <memory>

FrameData::FrameData(int ySize, int uvSize, int64_t pts, 
                     std::shared_ptr<uint8_t> poolPtr,
                     size_t poolOffset): 
    m_pts(pts),
    m_poolPtr(poolPtr),
    m_poolOffset(poolOffset)
{
    m_planeOffset[0] = 0;
    m_planeOffset[1] = ySize;
    m_planeOffset[2] = ySize + uvSize;
}

FrameData::~FrameData() = default;

uint8_t* FrameData::yPtr() const {
    if (!m_poolPtr) return nullptr;
    return m_poolPtr.get() + m_poolOffset + m_planeOffset[0];
}

uint8_t* FrameData::uPtr() const {
    if (!m_poolPtr) return nullptr;
    return m_poolPtr.get() + m_poolOffset + m_planeOffset[1];
}

uint8_t* FrameData::vPtr() const {
    if (!m_poolPtr) return nullptr;
    return m_poolPtr.get() + m_poolOffset + m_planeOffset[2];
}

int64_t FrameData::pts() const {
    return m_pts;
}

void FrameData::setPts(int64_t pts) {
    m_pts = pts;
}
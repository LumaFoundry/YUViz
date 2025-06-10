#include "frameMeta.h"
#include <cassert>


FrameMeta::FrameMeta(int width, int height, PixelFormat fmt,
                     AVRational timeBase,
                     AVRational sampleAspectRatio, 
                     ColorRange range,
                     ColorSpace space):
    m_yWidth(width), 
    m_yHeight(height), 
    m_fmt(fmt),
    m_timeBase(timeBase),
    m_sampleAspectRatio(sampleAspectRatio),
    m_colorRange(range),
    m_colorSpace(space)
{
    switch (m_fmt) {
    case PixelFormat::YUV420P:
        m_uvWidth = width / 2;
        m_uvHeight = height / 2;
        break;
    case PixelFormat::YUV422P:
        m_uvWidth = width / 2;
        m_uvHeight = height;
        break;
    case PixelFormat::YUV444P:
        m_uvWidth = width;
        m_uvHeight = height;
        break;
    }
}

FrameMeta::~FrameMeta() = default;

int FrameMeta::yWidth() const {
    return m_yWidth;
}

int FrameMeta::yHeight() const {
    return m_yHeight;
}

int FrameMeta::uvWidth() const {
    return m_uvWidth;
}

int FrameMeta::uvHeight() const {
    return m_uvHeight;
}

PixelFormat FrameMeta::format() const {
    return m_fmt;
}

AVRational FrameMeta::timeBase() const {
    return m_timeBase;
}

AVRational FrameMeta::sampleAspectRatio() const {
    return m_sampleAspectRatio;
}

ColorRange FrameMeta::colorRange() const {
    return m_colorRange;
}

ColorSpace FrameMeta::colorSpace() const {
    return m_colorSpace;
}
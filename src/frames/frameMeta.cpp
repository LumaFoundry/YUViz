#include "frameMeta.h"
#include <cassert>


FrameMeta::FrameMeta(int width, int height, 
                     AVPixelFormat fmt,
                     AVRational timeBase,
                     AVRational sampleAspectRatio, 
                     AVColorRange range,
                     AVColorSpace space):
    m_yWidth(width), 
    m_yHeight(height), 
    m_fmt(fmt),
    m_timeBase(timeBase),
    m_sampleAspectRatio(sampleAspectRatio),
    m_colorRange(range),
    m_colorSpace(space)
{
    switch (m_fmt) {
    case AVPixelFormat::AV_PIX_FMT_YUV420P:
        m_uvWidth = width / 2;
        m_uvHeight = height / 2;
        break;
    case AVPixelFormat::AV_PIX_FMT_YUV422P:
        m_uvWidth = width / 2;
        m_uvHeight = height;
        break;
    case AVPixelFormat::AV_PIX_FMT_YUV444P:
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

AVPixelFormat FrameMeta::format() const {
    return m_fmt;
}

AVRational FrameMeta::timeBase() const {
    return m_timeBase;
}

AVRational FrameMeta::sampleAspectRatio() const {
    return m_sampleAspectRatio;
}

AVColorRange FrameMeta::colorRange() const {
    return m_colorRange;
}

AVColorSpace FrameMeta::colorSpace() const {
    return m_colorSpace;
}
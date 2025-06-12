#pragma once

#include <cstdint>
#include <vector>
extern "C" {
#include <libavutil/rational.h>
#include <libavutil/pixfmt.h>
}

class FrameMeta {
public:
    FrameMeta(int width, int height, 
              AVPixelFormat fmt,
              AVRational timeBase,
              AVRational sampleAspectRatio,
              AVColorRange range, 
              AVColorSpace space);
    ~FrameMeta();

    int yWidth() const;
    int yHeight() const;
    int uvWidth() const;
    int uvHeight() const;

    int ySize() const {return yWidth() * yHeight(); };
    int uvSize() const {return uvWidth() * uvHeight(); };

    AVPixelFormat format() const;
    AVRational timeBase() const;
    AVRational sampleAspectRatio() const;
    AVColorRange colorRange() const;
    AVColorSpace colorSpace() const;

private:
    int m_yWidth;
    int m_yHeight;
    int m_uvWidth;
    int m_uvHeight;
    AVPixelFormat m_fmt;
    AVRational m_timeBase;
    AVRational m_sampleAspectRatio;
    AVColorRange m_colorRange;
    AVColorSpace m_colorSpace;
};
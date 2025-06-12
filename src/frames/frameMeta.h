#pragma once

#include <cstdint>
#include <vector>
extern "C" {
#include <libavutil/rational.h>
}

enum class PixelFormat {
    YUV420P,
    YUV422P,
    YUV444P
};

enum class ColorRange {
    Limited,
    Full
};

enum class ColorSpace {
    Rec601,
    Rec709,
    Rec2020,
    Unspecified
};

class FrameMeta {
public:
    FrameMeta(int width, int height, PixelFormat fmt,
              AVRational timeBase = AVRational{0,1},
              AVRational sampleAspectRatio = AVRational{1,1},
              ColorRange range = ColorRange::Limited,
              ColorSpace space = ColorSpace::Unspecified);
    ~FrameMeta();

    int yWidth() const;
    int yHeight() const;
    int uvWidth() const;
    int uvHeight() const;

    int ySize() const {return yWidth() * yHeight(); };
    int uvSize() const {return uvWidth() * uvHeight(); };

    PixelFormat format() const;
    AVRational timeBase() const;
    AVRational sampleAspectRatio() const;
    ColorRange colorRange() const;
    ColorSpace colorSpace() const;

private:
    int m_yWidth;
    int m_yHeight;
    int m_uvWidth;
    int m_uvHeight;
    PixelFormat m_fmt;
    AVRational m_timeBase;
    AVRational m_sampleAspectRatio;
    ColorRange m_colorRange;
    ColorSpace m_colorSpace;
};
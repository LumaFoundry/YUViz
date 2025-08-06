#pragma once

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

class FrameMeta {
  public:
    FrameMeta();
    ~FrameMeta();

    int yWidth() const;
    int yHeight() const;
    int uvWidth() const;
    int uvHeight() const;
    int ySize() const;
    int uvSize() const;
    int totalFrames() const;
    int64_t duration() const;

    AVPixelFormat format() const;
    AVRational timeBase() const;
    AVRational sampleAspectRatio() const;
    AVColorRange colorRange() const;
    AVColorSpace colorSpace() const;
    std::string filename() const;
    std::string codecName() const;

    void setYWidth(int width);
    void setYHeight(int height);
    void setUVWidth(int width);
    void setUVHeight(int height);
    void setPixelFormat(AVPixelFormat fmt);
    void setTimeBase(AVRational timeBase);
    void setSampleAspectRatio(AVRational sampleAspectRatio);
    void setColorRange(AVColorRange range);
    void setColorSpace(AVColorSpace space);
    void setFilename(const std::string& filename);
    void setCodecName(const std::string& codecName);
    void setDuration(int64_t msDuration);
    void setTotalFrames(int totalFrames);

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
    std::string m_filename;
    std::string m_codecName;
    int64_t m_durationMs;
    int m_totalFrames;
};
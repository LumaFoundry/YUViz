#pragma once

#include <QFileInfo>
#include <QObject>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "frameQueue.h"
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "utils/errorReporter.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class VideoDecoder : public QObject {
    Q_OBJECT

  public:
    VideoDecoder(QObject* parent = nullptr);
    virtual ~VideoDecoder();
    void setDimensions(int width, int height);
    void setFramerate(double framerate);
    void setFormat(AVPixelFormat format);
    void setFileName(const std::string& fileName);
    void setFrameQueue(std::shared_ptr<FrameQueue> frameQueue);
    void setForceSoftwareDecoding(bool force);

    void openFile();
    virtual FrameMeta getMetaData();

    int64_t getDurationMs();
    int getTotalFrames();

  public slots:
    virtual void loadFrames(int num_frames, int direction);
    virtual void seek(int64_t timestamp);

  signals:
    void framesLoaded(bool success);
    void frameSeeked(int64_t pts);

  private:
    AVFormatContext* formatContext;
    AVCodecContext* codecContext;
    AVDictionary* inputOptions;
    int videoStreamIndex;

    FrameMeta metadata;
    int currentFrameIndex = 0;
    int localTail = -1;

    int64_t m_ptsOffset = -1;

    int m_width;
    int m_height;
    double m_framerate;
    AVPixelFormat m_format;
    std::string m_fileName;
    std::shared_ptr<FrameQueue> m_frameQueue;

    int yuvTotalFrames = -1;
    bool m_forceSoftwareDecoding = false;

    AVBufferRef* hw_device_ctx = nullptr;
    AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;

    void closeFile();

    bool isYUV(AVCodecID codecId);
    bool initializeHardwareDecoder(AVHWDeviceType deviceType, AVPixelFormat pixFmt);
    int64_t loadYUVFrame();
    void copyFrame(AVPacket*& tempPacket, FrameData* frameData, int& retFlag);
    int64_t loadCompressedFrame();

    bool m_hitEndFrame = false;
    bool m_needsTimebaseConversion = false;
    bool m_wait = true;

    void seekTo(int64_t targetPts);
    void seekToYUV(int64_t targetPts);
    void seekToCompressed(int64_t targetPts);
};

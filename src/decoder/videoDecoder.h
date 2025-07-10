#pragma once

#include <string>
#include <fstream>
#include <QObject>
#include <QFileInfo>
#include <iostream>
#include <cstring>

#include "frameQueue.h"
#include "frames/frameMeta.h"
#include "frames/frameData.h"
#include "utils/errorReporter.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
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

	void openFile();
    virtual FrameMeta getMetaData();

	int64_t getDurationMs();
	int getTotalFrames();

public slots:
    virtual void loadFrames(int num_frames);
	virtual void seek(int64_t timestamp);
	virtual void loadPreviousFrames(int num_frames);

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
	
	int m_width;
	int m_height;
	double m_framerate;
	AVPixelFormat m_format;
	std::string m_fileName;
	std::shared_ptr<FrameQueue> m_frameQueue;

	int yuvTotalFrames = -1;

	void closeFile();
	
	bool isYUV(AVCodecID codecId);
    int64_t loadYUVFrame();
    void copyFrame(AVPacket *&tempPacket, FrameData *frameData, int &retFlag);
    int64_t loadCompressedFrame();

	bool m_hitEndFrame = false;

};

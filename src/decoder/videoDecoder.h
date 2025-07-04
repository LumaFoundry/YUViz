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
	void setFrameQueue(FrameQueue* frameQueue);

	void openFile();
    virtual FrameMeta getMetaData();

public slots:
    virtual void loadFrame(int num_frames);

signals:
    void frameLoaded(bool success);

private:
	AVFormatContext* formatContext;
	AVCodecContext* codecContext;
	AVDictionary* inputOptions;
	int videoStreamIndex;
	
	FrameMeta metadata;
	int currentFrameIndex;
	
	int m_width;
	int m_height;
	double m_framerate;
	AVPixelFormat m_format;
	std::string m_fileName;
	FrameQueue* m_frameQueue;

	int yuvTotalFrames = -1;

	void closeFile();
	
	bool isYUV(AVCodecID codecId);
    void loadYUVFrame(FrameData *frameData);
    void copyFrame(AVPacket *&tempPacket, FrameData *frameData, int &retFlag);
    void loadCompressedFrame(FrameData* frameData);

};

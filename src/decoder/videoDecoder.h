#include <string>
#include <fstream>
#include <QObject>
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
	void setWidth(int width);
	void setHeight(int height);
	void setFramerate(double framerate);
	void setFormat(AVPixelFormat format);
	void setFileName(const std::string& fileName);

	void openFile();
    FrameMeta getMetaData();

public slots:
    void loadFrame(FrameData* frame);

signals:
    void frameLoaded(bool success);

private:
	AVFormatContext* formatContext;
	AVCodecContext* codecContext;
	int videoStreamIndex;
	
	FrameMeta metadata;
	int currentFrameIndex;
	
	int m_width;
	int m_height;
	double m_framerate;
	AVPixelFormat m_format;
	std::string m_fileName;

	void closeFile();
	
	bool isYUV(AVCodecID codecId);
    void loadYUVFrame(FrameData *frameData);
    void copyFrame(AVPacket *&tempPacket, FrameData *frameData, int &retFlag);
    void loadCompressedFrame(FrameData* frameData);

};

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

	// Setters for video properties (kept for backward compatibility)
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
	// FFmpeg structures
	AVFormatContext* formatContext;
	AVCodecContext* codecContext;
	int videoStreamIndex;
	
	// Metadata and state
	FrameMeta metadata;
	int currentFrameIndex;
	
	// Video properties (for backward compatibility or override)
	int m_width;
	int m_height;
	double m_framerate;
	AVPixelFormat m_format;
	std::string m_fileName;

	void closeFile();
	
	// Helper methods for YUV format detection and loading
	bool isRawYUVCodec(AVCodecID codecId);
	void loadRawYUVFrame(FrameData* frameData);
	void loadCompressedFrame(FrameData* frameData);

};

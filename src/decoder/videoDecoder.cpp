#include "videoDecoder.h"
#include <iostream>
#include <cstring>

extern "C" {
#include <libavutil/rational.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

VideoDecoder::VideoDecoder(QObject* parent) : 
    QObject(parent),
    formatContext(nullptr),
    codecContext(nullptr),
    inputOptions(nullptr),
    videoStreamIndex(-1),
    currentFrameIndex(0),
    m_width(1920),
    m_height(1080),
    m_framerate(25.0),
    m_format(AVPixelFormat::AV_PIX_FMT_YUV420P),
    m_fileName(""),
    metadata()
{
}

VideoDecoder::~VideoDecoder()
{
	closeFile();
}

void VideoDecoder::setDimensions(int width, int height)
{
    m_width = width;
    m_height = height;
    av_dict_set(&inputOptions, "video_size", (std::to_string(m_width) + "x" + std::to_string(m_height)).c_str(), 0);
}


void VideoDecoder::setFramerate(double framerate)
{
    m_framerate = framerate;
    av_dict_set(&inputOptions, "framerate", std::to_string(m_framerate).c_str(), 0);
}

void VideoDecoder::setFormat(AVPixelFormat format)
{
    m_format = format;
}

void VideoDecoder::setFileName(const std::string& fileName)
{
    m_fileName = fileName;
}

/**
 * @brief Opens a video file for decoding and initializes FrameMeta object.
 */
void VideoDecoder::openFile()
{
    // Close any previously opened file
    closeFile();

    //av_dict_set(&input_options, "framerate", std::to_string(m_framerate).c_str(), 0);
    //av_dict_set(&input_options, "pixel_format", "yuv420p", 0);

    // Open input file
    if (avformat_open_input(&formatContext, m_fileName.c_str(), nullptr, &inputOptions) < 0) {
        ErrorReporter::instance().report("Could not open input file " + m_fileName, LogLevel::Error);
        return;
    }
    
    // Retrieve stream information
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        ErrorReporter::instance().report("Could not find stream information", LogLevel::Error);
        closeFile();
        return;
    }
    
    // Find the video stream
    videoStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        ErrorReporter::instance().report("Could not find video stream", LogLevel::Error);
        closeFile();
        return;
    }
    
    AVStream* videoStream = formatContext->streams[videoStreamIndex];
    
    // Find decoder for the video stream
    const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec) {
        ErrorReporter::instance().report("Unsupported codec", LogLevel::Error);
        closeFile();
        return;
    }
    
    // Allocate codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        ErrorReporter::instance().report("Could not allocate codec context", LogLevel::Error);
        closeFile();
        return;
    }
    
    // Copy codec parameters to context
    if (avcodec_parameters_to_context(codecContext, videoStream->codecpar) < 0) {
        ErrorReporter::instance().report("Could not copy codec parameters to context", LogLevel::Error);
        closeFile();
        return;
    }
    
    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        ErrorReporter::instance().report("Could not open codec", LogLevel::Error);
        closeFile();
        return;
    }
    
    AVRational timeBase;
    // For raw YUV, or if videoStream->time_base doesn't exist, calculate timebase from framerate (fps)
    if (isYUV(codecContext->codec_id) 
    || !videoStream->time_base.num || !videoStream->time_base.den) {
        timeBase = av_d2q(1.0 / m_framerate, 1000000);
    } else {
        timeBase = videoStream->time_base;
    }

    // Calculate Y and UV dimensions using FFmpeg pixel format descriptor
    const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(codecContext->pix_fmt);
    // int yWidth = codecContext->width;
    // int yHeight = codecContext->height;
    int uvWidth = AV_CEIL_RSHIFT(m_width, pixDesc->log2_chroma_w);
    int uvHeight = AV_CEIL_RSHIFT(m_height, pixDesc->log2_chroma_h);
    
    metadata.setYWidth(m_width);
    metadata.setYHeight(m_height);
    metadata.setUVWidth(uvWidth);
    metadata.setUVHeight(uvHeight);
    metadata.setPixelFormat(codecContext->pix_fmt);
    metadata.setTimeBase(timeBase);
    metadata.setSampleAspectRatio(videoStream->sample_aspect_ratio);
    metadata.setColorRange(codecContext->color_range);
    metadata.setColorSpace(codecContext->colorspace);
    metadata.setFilename(m_fileName);
    
    currentFrameIndex = 0;

    return;
    }

/**
 * @brief Loads a video frame into the decoder.
 *
 * This function attempts to load a frame using the provided FrameData pointer.
 * It first checks if the decoder is properly initialized and emits a signal if not.
 * If the codec is a raw YUV format, it loads the frame directly without decoding.
 * Otherwise, it uses the standard decoding path for compressed codecs.
 *
 * @param frameData Pointer to the FrameData structure containing frame information.
 *
 * @note Emits the frameLoaded(bool) signal to indicate success or failure.
 */
void VideoDecoder::loadFrame(FrameData* frameData)
{

    // qDebug() << "VideoDecoder::loadFrame called with frameData: " << frameData;

    if (!formatContext || !codecContext || !frameData) {
        ErrorReporter::instance().report("VideoDecoder not properly initialized", LogLevel::Error);
        emit frameLoaded(false);
    }

    // Check if this is a raw YUV format that doesn't need decoding
    bool isRawYUV = isYUV(codecContext->codec_id);
    
    if (isRawYUV) {
        loadYUVFrame(frameData);
    } else{
        // For compressed codecs, use the standard decoding path
        loadCompressedFrame(frameData);
    }

}

FrameMeta VideoDecoder::getMetaData()
{
    return metadata;
}

void VideoDecoder::closeFile()
{
    if (codecContext) {
        avcodec_free_context(&codecContext);
        codecContext = nullptr;
    }
    
    if (formatContext) {
        avformat_close_input(&formatContext);
        formatContext = nullptr;
    }
    
    videoStreamIndex = -1;
    currentFrameIndex = 0;
}

bool VideoDecoder::isYUV(AVCodecID codecId)
{
    switch (codecId) {
        case AV_CODEC_ID_RAWVIDEO:
        case AV_CODEC_ID_YUV4:
            return true;
        default:
            return false;
    }
}

void VideoDecoder::loadYUVFrame(FrameData* frameData)
{
    AVPacket* tempPacket = av_packet_alloc();
    if (!tempPacket) {
        ErrorReporter::instance().report("Could not allocate packet", LogLevel::Error);
        emit frameLoaded(false);
        return;
    }
    int ret;
    while ((ret = av_read_frame(formatContext, tempPacket)) >= 0) {
        if (tempPacket->stream_index == videoStreamIndex) {
            int retFlag;
            copyFrame(tempPacket, frameData, retFlag);
            if (retFlag == 2)
                break;
            return;
        }
    }

    if (ret < 0 && ret != AVERROR_EOF) {
        ErrorReporter::instance().report("Failed to read raw YUV frame", LogLevel::Error);
    }

    av_packet_free(&tempPacket);
    emit frameLoaded(false);
    return;
}

void VideoDecoder::loadCompressedFrame(FrameData* frameData)
{
    // Allocate packet for decoding operation
    AVPacket* tempPacket = av_packet_alloc();
    if (!tempPacket) {
        ErrorReporter::instance().report("Could not allocate packet", LogLevel::Error);
        emit frameLoaded(false);
        return;
    }
    
    int ret;
    
    // Read frames until we get a video frame
    while ((ret = av_read_frame(formatContext, tempPacket)) >= 0) {
        if (tempPacket->stream_index == videoStreamIndex) {
            // Send packet to decoder
            ret = avcodec_send_packet(codecContext, tempPacket);
            if (ret < 0) {
                ErrorReporter::instance().report("Failed to send packet to decoder", LogLevel::Error);
                av_packet_unref(tempPacket);
                break;
            }
            
            // Allocate a temporary frame for decoding
            AVFrame* tempFrame = av_frame_alloc();
            if (!tempFrame) {
                ErrorReporter::instance().report("Could not allocate temporary frame", LogLevel::Error);
                av_packet_unref(tempPacket);
                break;
            }
            
            ret = avcodec_receive_frame(codecContext, tempFrame);

            if (ret == 0) {
                int retFlag;
                copyFrame(tempPacket, frameData, retFlag);
                if (retFlag == 2)
                    break;
                return;
            }
        }
    }
    
    if (ret < 0 && ret != AVERROR_EOF) {
        ErrorReporter::instance().report("Failed to read frame", LogLevel::Error);
    }
    
    // Clean up allocations
    av_packet_free(&tempPacket);
    emit frameLoaded(false);
    return;
}

/**
 * @brief Copies the decoded frame data into the provided FrameData structure.
 *
 * This function handles the conversion of the decoded frame data into a planar YUV format
 * and populates the FrameData structure with the Y, U, and V plane pointers.
 *
 * @param tempPacket Pointer to the AVPacket containing the decoded frame data.
 * @param frameData Pointer to the FrameData structure to populate.
 * @param retFlag Reference to an integer flag indicating success or failure of the operation.
 */
void VideoDecoder::copyFrame(AVPacket *&tempPacket, FrameData *frameData, int &retFlag)
{
    retFlag = 1;
    uint8_t *packetData = tempPacket->data;

    AVPixelFormat srcFmt = metadata.format();
    int width = metadata.yWidth();
    int height = metadata.yHeight();

    // Prepare source pointers and line sizes
    uint8_t *srcData[4] = {nullptr};
    int srcLinesize[4] = {0};
    av_image_fill_arrays(srcData, srcLinesize, packetData, srcFmt, width, height, 1);

    // Prepare destination pointers and line sizes (planar YUV420P)
    uint8_t *dstData[4] = {frameData->yPtr(), frameData->uPtr(), frameData->vPtr(), nullptr};
    int dstLinesize[4] = {width, width / 2, width / 2, 0};
    SwsContext *swsCtx = sws_getContext(
        width, height, srcFmt,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_POINT, nullptr, nullptr, nullptr);

    if (!swsCtx)
    {
        ErrorReporter::instance().report("Failed to create swsContext for YUV conversion", LogLevel::Error);
        av_packet_unref(tempPacket);
        {
            retFlag = 2;
            return;
        };
    }

    // Scale and convert the YUV frame
    int res = sws_scale(swsCtx, srcData, srcLinesize, 0, height, dstData, dstLinesize);
    sws_freeContext(swsCtx);

    if (res <= 0)
    {
        ErrorReporter::instance().report("sws_scale failed to convert/copy YUV frame", LogLevel::Error);
        av_packet_unref(tempPacket);
        {
            retFlag = 2;
            return;
        };
    }

    frameData->setPts(tempPacket->pts);
    av_packet_unref(tempPacket);
    currentFrameIndex++;
    av_packet_free(&tempPacket);

    // qDebug() << "VideoDecoder:: emit frameLoaded";
    emit frameLoaded(true);
    return;
}


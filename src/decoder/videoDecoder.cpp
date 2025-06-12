#include "videoDecoder.h"
#include <iostream>
#include <cstring>

extern "C" {
#include <libavutil/rational.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
}

VideoDecoder::VideoDecoder(QObject* parent) : 
    QObject(parent),
    formatContext(nullptr),
    codecContext(nullptr),
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

void VideoDecoder::setWidth(int width)
{
    m_width = width;
}

void VideoDecoder::setHeight(int height)
{
    m_height = height;
}

void VideoDecoder::setFramerate(double framerate)
{
    m_framerate = framerate;
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
    
    // Open input file
    if (avformat_open_input(&formatContext, m_fileName.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Error: Could not open input file " << m_fileName << std::endl;
        return;
    }
    
    // Retrieve stream information
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Error: Could not find stream information" << std::endl;
        closeFile();
        return;
    }
    
    // Find the video stream
    videoStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        std::cerr << "Error: Could not find video stream" << std::endl;
        closeFile();
        return;
    }
    
    AVStream* videoStream = formatContext->streams[videoStreamIndex];
    
    // Find decoder for the video stream
    const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec) {
        std::cerr << "Error: Unsupported codec" << std::endl;
        closeFile();
        return;
    }
    
    // Allocate codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Error: Could not allocate codec context" << std::endl;
        closeFile();
        return;
    }
    
    // Copy codec parameters to context
    if (avcodec_parameters_to_context(codecContext, videoStream->codecpar) < 0) {
        std::cerr << "Error: Could not copy codec parameters to context" << std::endl;
        closeFile();
        return;
    }
    
    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Error: Could not open codec" << std::endl;
        closeFile();
        return;
    }
    
    AVRational timeBase;
    // For raw YUV, or if videoStream->time_base doesn't exist, calculate timebase from framerate (fps)
    if (isRawYUVCodec(codecContext->codec_id) 
    || !videoStream->time_base.num || !videoStream->time_base.den) {
        timeBase = av_d2q(1.0 / m_framerate, 1000000);
    } else {
        timeBase = videoStream->time_base;
    }

    // Calculate Y and UV dimensions using FFmpeg pixel format descriptor
    const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(codecContext->pix_fmt);
    int yWidth = codecContext->width;
    int yHeight = codecContext->height;
    int uvWidth = AV_CEIL_RSHIFT(yWidth, pixDesc->log2_chroma_w);
    int uvHeight = AV_CEIL_RSHIFT(yHeight, pixDesc->log2_chroma_h);
    
    metadata.setYWidth(yWidth);
    metadata.setYHeight(yHeight);
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
    if (!formatContext || !codecContext || !frameData) {
        std::cerr << "Error: VideoDecoder not properly initialized" << std::endl;
        emit frameLoaded(false);
    }
    
    // Check if this is a raw YUV format that doesn't need decoding
    bool isRawYUV = isRawYUVCodec(codecContext->codec_id);
    
    if (isRawYUV) {
        loadRawYUVFrame(frameData);
    }
    
    // For compressed codecs, use the standard decoding path
    loadCompressedFrame(frameData);
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

bool VideoDecoder::isRawYUVCodec(AVCodecID codecId)
{
    switch (codecId) {
        case AV_CODEC_ID_RAWVIDEO:
        case AV_CODEC_ID_YUV4:
            return true;
        default:
            return false;
    }
}

void VideoDecoder::loadRawYUVFrame(FrameData* frameData)
{
    // Allocate packet for reading raw data
    AVPacket* tempPacket = av_packet_alloc();
    if (!tempPacket) {
        std::cerr << "Error: Could not allocate packet" << std::endl;
        emit frameLoaded(false);
        return;
    }
    
    int ret;
    
    // Read frames until we get a video frame
    while ((ret = av_read_frame(formatContext, tempPacket)) >= 0) {
        if (tempPacket->stream_index == videoStreamIndex) {
            // For raw YUV, the packet data contains the raw frame data
            // Copy data directly from packet to FrameData buffers
            uint8_t* packetData = tempPacket->data;
            int packetSize = tempPacket->size;
            
            // Calculate plane sizes
            int ySize = metadata.ySize();
            int uSize = metadata.uvSize();
            int vSize = metadata.uvSize();
            
            if (packetSize < ySize + uSize + vSize) {
                std::cerr << "Error: Packet size too small for frame data" << std::endl;
                av_packet_unref(tempPacket);
                break;
            }
            
            // Copy Y plane
            memcpy(frameData->yPtr(), packetData, ySize);
            
            // Copy U plane
            memcpy(frameData->uPtr(), packetData + ySize, uSize);
            
            // Copy V plane
            memcpy(frameData->vPtr(), packetData + ySize + uSize, vSize);
            
            // Set presentation timestamp
            frameData->setPts(tempPacket->pts);
            
            av_packet_unref(tempPacket);
            currentFrameIndex++;
            
            // Clean up allocations
            av_packet_free(&tempPacket);
            emit frameLoaded(true);
            return;
        }
    }
    
    if (ret < 0 && ret != AVERROR_EOF) {
        std::cerr << "Error: Failed to read raw YUV frame" << std::endl;
    }
    
    // Clean up allocations
    av_packet_free(&tempPacket);
    emit frameLoaded(false);
    return;;
}

void VideoDecoder::loadCompressedFrame(FrameData* frameData)
{
    // Allocate packet for this decoding operation
    AVPacket* tempPacket = av_packet_alloc();
    if (!tempPacket) {
        std::cerr << "Error: Could not allocate packet" << std::endl;
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
                std::cerr << "Error: Failed to send packet to decoder" << std::endl;
                av_packet_unref(tempPacket);
                break;
            }
            
            // Allocate a temporary frame for decoding
            AVFrame* tempFrame = av_frame_alloc();
            if (!tempFrame) {
                std::cerr << "Error: Could not allocate temporary frame" << std::endl;
                av_packet_unref(tempPacket);
                break;
            }
            
            // Receive frame from decoder
            ret = avcodec_receive_frame(codecContext, tempFrame);
            if (ret == 0) {
                // Successfully decoded a frame - copy data to our FrameData buffers
                
                // Copy Y plane
                uint8_t* yDst = frameData->yPtr();
                uint8_t* ySrc = tempFrame->data[0];
                int yHeight = codecContext->height;
                int yLinesize = tempFrame->linesize[0];
                int yWidth = codecContext->width;
                
                for (int y = 0; y < yHeight; y++) {
                    memcpy(yDst + y * yWidth, ySrc + y * yLinesize, yWidth);
                }
                
                // Copy U plane
                uint8_t* uDst = frameData->uPtr();
                uint8_t* uSrc = tempFrame->data[1];
                int uvHeight = metadata.uvHeight();
                int uvWidth = metadata.uvWidth();
                int uLinesize = tempFrame->linesize[1];
                
                for (int y = 0; y < uvHeight; y++) {
                    memcpy(uDst + y * uvWidth, uSrc + y * uLinesize, uvWidth);
                }
                
                // Copy V plane
                uint8_t* vDst = frameData->vPtr();
                uint8_t* vSrc = tempFrame->data[2];
                int vLinesize = tempFrame->linesize[2];
                
                for (int y = 0; y < uvHeight; y++) {
                    memcpy(vDst + y * uvWidth, vSrc + y * vLinesize, uvWidth);
                }
                
                // Set presentation timestamp
                frameData->setPts(tempFrame->pts);
                
                av_packet_unref(tempPacket);
                av_frame_free(&tempFrame);
                currentFrameIndex++;
                
                // Clean up allocations
                av_packet_free(&tempPacket);
                emit frameLoaded(true);
                return;
            } else if (ret == AVERROR(EAGAIN)) {
                // Need more input
                av_frame_free(&tempFrame);
                av_packet_unref(tempPacket);
                continue;
            } else {
                std::cerr << "Error: Failed to receive frame from decoder" << std::endl;
                av_frame_free(&tempFrame);
                av_packet_unref(tempPacket);
                break;
            }
        }
        av_packet_unref(tempPacket);
    }
    
    if (ret < 0 && ret != AVERROR_EOF) {
        std::cerr << "Error: Failed to read frame" << std::endl;
    }
    
    // Clean up allocations
    av_packet_free(&tempPacket);
    emit frameLoaded(false);
    return;
}


#include "videoDecoder.h"
#include <chrono>
#include <thread>

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
    metadata() {
}

VideoDecoder::~VideoDecoder() {
    closeFile();
}

void VideoDecoder::setDimensions(int width, int height) {
    m_width = width;
    m_height = height;
    av_dict_set(&inputOptions, "video_size", (std::to_string(m_width) + "x" + std::to_string(m_height)).c_str(), 0);
}

void VideoDecoder::setFramerate(double framerate) {
    m_framerate = framerate;
    av_dict_set(&inputOptions, "framerate", std::to_string(m_framerate).c_str(), 0);
}

void VideoDecoder::setFormat(AVPixelFormat format) {
    m_format = format;
    const char* pixFmtString = av_get_pix_fmt_name(m_format);
    av_dict_set(&inputOptions, "pixel_format", pixFmtString, 0);
}

void VideoDecoder::setFileName(const std::string& fileName) {
    m_fileName = fileName;
}

void VideoDecoder::setFrameQueue(std::shared_ptr<FrameQueue> frameQueue) {
    m_frameQueue = frameQueue;
}

/**
 * @brief Opens a video file for decoding and initializes FrameMeta object.
 */
void VideoDecoder::openFile() {
    // Close any previously opened file
    closeFile();

    // For raw YUV files, we need to specify the input format
    const AVInputFormat* inputFormat = nullptr;

    // Check if this is likely a raw YUV file based on file extension
    std::string fileName = m_fileName;
    if (fileName.find(".yuv") != std::string::npos || fileName.find(".raw") != std::string::npos) {
        inputFormat = av_find_input_format("rawvideo");
        qDebug() << "VideoDecoder: Detected raw YUV file, using rawvideo input format";
    }

    // av_dict_set(&input_options, "framerate", std::to_string(m_framerate).c_str(), 0);
    // av_dict_set(&input_options, "pixel_format", "yuv420p", 0);

    // Open input file
    if (avformat_open_input(&formatContext, m_fileName.c_str(), inputFormat, &inputOptions) < 0) {
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

    // Find decoder for the video stream, try hardware decoding if possible
    const AVCodec* codec = nullptr;
    AVCodecID codecId = videoStream->codecpar->codec_id;

    // Find the standard decoder first
    codec = avcodec_find_decoder(codecId);
    if (!codec) {
        qDebug() << "✗ No decoder found for codec:" << avcodec_get_name(codecId);
        return;
    }

    qDebug() << "Found decoder:" << codec->name << "for codec:" << avcodec_get_name(codecId);

    // Try to enable hardware acceleration (only for compressed formats)
    if (isYUV(codecId)) {
        qDebug() << "RAW/YUV format detected - hardware acceleration not applicable";
    } else {
        // Only try hardware acceleration for compressed formats
#if defined(Q_OS_MACOS)
        if (initializeHardwareDecoder(AV_HWDEVICE_TYPE_VIDEOTOOLBOX, AV_PIX_FMT_VIDEOTOOLBOX)) {
            qDebug() << "HARDWARE ACCELERATION ENABLED: VideoToolbox for" << codec->name;
        } else {
            qDebug() << "VideoToolbox hardware acceleration not available, using software decoding";
        }
#elif defined(Q_OS_LINUX)
        if (initializeHardwareDecoder(AV_HWDEVICE_TYPE_VAAPI, AV_PIX_FMT_VAAPI)) {
            qDebug() << "HARDWARE ACCELERATION ENABLED: VAAPI for" << codec->name;
        } else {
            qDebug() << "VAAPI hardware acceleration not available, using software decoding";
        }
#elif defined(Q_OS_WIN)
        if (initializeHardwareDecoder(AV_HWDEVICE_TYPE_D3D11VA, AV_PIX_FMT_D3D11)) {
            qDebug() << "HARDWARE ACCELERATION ENABLED: D3D11VA for" << codec->name;
        } else {
            qDebug() << "D3D11VA hardware acceleration not available, using software decoding";
        }
#else
        qDebug() << "No hardware acceleration available on this platform";
#endif
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

    // Bind hardware device context if present
    if (hw_device_ctx) {
        codecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    }

    if (!isYUV(codecContext->codec_id)) {
        m_width = codecContext->width;
        m_height = codecContext->height;
        setFramerate(videoStream->avg_frame_rate.num / (double)videoStream->avg_frame_rate.den);
    }

    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        ErrorReporter::instance().report("Could not open codec", LogLevel::Error);
        closeFile();
        return;
    }

    // Print final decoder status based on actual codec and hardware context
    bool isActuallyUsingHardware = (hw_device_ctx != nullptr) && (codecContext->hw_device_ctx != nullptr);

    if (isYUV(codecId)) {
        qDebug() << "FINAL STATUS: RAW format processing -" << codec->name << "(no decoding required)";
    } else if (isActuallyUsingHardware) {
#if defined(Q_OS_MACOS)
        qDebug() << "FINAL STATUS: Hardware decoding active -" << codec->name << "with VideoToolbox";
#elif defined(Q_OS_LINUX)
        qDebug() << "FINAL STATUS: Hardware decoding active -" << codec->name << "with VAAPI";
#elif defined(Q_OS_WIN)
        qDebug() << "FINAL STATUS: Hardware decoding active -" << codec->name << "with D3D11VA";
#endif
    } else {
        qDebug() << "FINAL STATUS: Software decoding active -" << codec->name;
    }

    AVRational timeBase;
    AVRational targetTimebase = av_d2q(1.0 / m_framerate, 1000000);
    qDebug() << "TIMEBASE: " << videoStream->time_base.num << "/" << videoStream->time_base.den;
    qDebug() << "TARGET TIMEBASE: " << targetTimebase.num << "/" << targetTimebase.den;
    // For raw YUV, or if videoStream->time_base doesn't exist, calculate timebase from framerate (fps)
    if (timeBase.den != targetTimebase.den) {
        // timeBase = targetTimebase;
        m_needsTimebaseConversion = true;
    }

    // Calculate Y and UV dimensions using FFmpeg pixel format descriptor
    const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(codecContext->pix_fmt);
    // int yWidth = codecContext->width;
    // int yHeight = codecContext->height;
    int uvWidth = AV_CEIL_RSHIFT(m_width, pixDesc->log2_chroma_w);
    int uvHeight = AV_CEIL_RSHIFT(m_height, pixDesc->log2_chroma_h);

    setDimensions(m_width, m_height);
    metadata.setYWidth(m_width);
    metadata.setYHeight(m_height);
    metadata.setUVWidth(uvWidth);
    metadata.setUVHeight(uvHeight);
    metadata.setPixelFormat(codecContext->pix_fmt);
    metadata.setTimeBase(targetTimebase);
    metadata.setSampleAspectRatio(videoStream->sample_aspect_ratio);
    metadata.setColorRange(codecContext->color_range);
    metadata.setColorSpace(codecContext->colorspace);
    metadata.setFilename(m_fileName);
    metadata.setDuration(getDurationMs());
    metadata.setTotalFrames(getTotalFrames());
    setFormat(codecContext->pix_fmt);

    if (isYUV(codecContext->codec_id)) {
        int ySize = m_width * m_height;
        int uvSize = uvWidth * uvHeight;
        int frameSize = ySize + 2 * uvSize;

        QFileInfo info(QString::fromStdString(m_fileName));
        int64_t fileSize = info.size();
        yuvTotalFrames = fileSize / frameSize;
    }

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
void VideoDecoder::loadFrames(int num_frames, int direction = 1) {
    if (m_wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_wait = false;
    }

    if (num_frames == 0) {
        emit framesLoaded(true);
        return;
    }
    // qDebug() << "VideoDecoder::loadFrame called with frameData: " << frameData;

    if (!formatContext || !codecContext) {
        ErrorReporter::instance().report("VideoDecoder not properly initialized", LogLevel::Error);
        emit framesLoaded(false);
    }

    bool isRawYUV = isYUV(codecContext->codec_id);
    int64_t maxpts = -1;
    int64_t minpts = INT64_MAX;

    // Seek to correct frame before loading
    if (direction == -1) {
        if (currentFrameIndex == 0) {
            qDebug() << "At the beginning of the video, cannot seek backward";
            m_frameQueue->updateTail(0);
            ErrorReporter::instance().report("Cannot seek backward", LogLevel::Warning);
            emit framesLoaded(false);
            return;
        }
        currentFrameIndex -= num_frames + 1;
        if (currentFrameIndex < 0) {
            currentFrameIndex = 0;
            direction = 1; // Change direction to forward if we hit the beginning
        }
        seekTo(currentFrameIndex);
        qDebug() << "VideoDecoder::seeking to " << currentFrameIndex;
        // Make sure we don't load more than half of the queue size
        num_frames = std::min(num_frames, m_frameQueue->getSize() / 2);
    }

    localTail = currentFrameIndex;

    qDebug() << "VideoDecoder::loadFrames called with num_frames: " << num_frames << ", direction: " << direction
             << ", currentFrameIndex: " << currentFrameIndex;

    for (int i = 0; i < num_frames; ++i) {
        int64_t temp_pts;
        if (isRawYUV) {
            temp_pts = loadYUVFrame();
        } else {
            temp_pts = loadCompressedFrame();
            // qDebug() << "VideoDecoder::loadCompressedFrame returned pts: " << temp_pts;
        }

        // Check if we've reached EOF (indicated by -1 PTS)
        if (temp_pts == -1) {
            // qDebug() << "VideoDecoder: Reached EOF, marking last frame as end frame";
            if (currentFrameIndex > 0) {
                // For compressed video, check if we have a valid total frame count
                int totalFrames = getTotalFrames();
                int64_t lastFramePts = currentFrameIndex - 1;

                // If we have a total frame count and we're near it, use that as the last frame
                if (totalFrames > 0 && lastFramePts < totalFrames - 1) {
                    lastFramePts = totalFrames - 1;
                }

                FrameData* lastFrame = m_frameQueue->getTailFrame(lastFramePts);
                if (lastFrame) {
                    lastFrame->setEndFrame(true);
                    // qDebug() << "VideoDecoder: Marked frame " << lastFramePts
                    //          << " as end frame (total frames: " << totalFrames << ")";
                }
            }
            break;
        }

        // Also check if we're at the last frame for compressed videos
        if (!isRawYUV) {
            int totalFrames = getTotalFrames();
            if (totalFrames > 0 && temp_pts >= totalFrames - 1) {
                FrameData* endFrame = m_frameQueue->getTailFrame(temp_pts);
                if (endFrame) {
                    endFrame->setEndFrame(true);
                    // qDebug() << "VideoDecoder: Marked frame " << temp_pts
                    //          << " as end frame (reached total frames: " << totalFrames << ")";
                }
            }
        }

        maxpts = std::max(maxpts, temp_pts);
        minpts = std::min(minpts, std::max(temp_pts, int64_t{0}));
    }

    qDebug() << "VideoDecoder:: Loaded from " << localTail << " to " << currentFrameIndex;

    if (direction == 1) {
        m_frameQueue->updateTail(maxpts);
    } else {
        m_frameQueue->updateTail(minpts);
    }

    emit framesLoaded(true);
}

FrameMeta VideoDecoder::getMetaData() {
    return metadata;
}

void VideoDecoder::closeFile() {
    if (codecContext) {
        avcodec_free_context(&codecContext);
        codecContext = nullptr;
    }

    if (formatContext) {
        avformat_close_input(&formatContext);
        formatContext = nullptr;
    }

    if (hw_device_ctx) {
        av_buffer_unref(&hw_device_ctx);
        hw_device_ctx = nullptr;
    }

    hw_pix_fmt = AV_PIX_FMT_NONE;
    videoStreamIndex = -1;
    currentFrameIndex = 0;
}

bool VideoDecoder::isYUV(AVCodecID codecId) {
    switch (codecId) {
    case AV_CODEC_ID_RAWVIDEO:
    case AV_CODEC_ID_YUV4:
        return true;
    default:
        return false;
    }
}

bool VideoDecoder::initializeHardwareDecoder(AVHWDeviceType deviceType, AVPixelFormat pixFmt) {
    if (av_hwdevice_ctx_create(&hw_device_ctx, deviceType, nullptr, nullptr, 0) < 0) {
        const char* deviceName = av_hwdevice_get_type_name(deviceType);
        ErrorReporter::instance().report(
            QString("Failed to create %1 device").arg(deviceName ? deviceName : "unknown").toStdString(),
            LogLevel::Warning);
        return false;
    }

    hw_pix_fmt = pixFmt;
    qDebug() << "Successfully initialized hardware decoder:" << av_hwdevice_get_type_name(deviceType);
    return true;
}

int64_t VideoDecoder::loadYUVFrame() {
    AVPacket* tempPacket = av_packet_alloc();
    if (!tempPacket) {
        ErrorReporter::instance().report("Could not allocate packet", LogLevel::Error);
        emit framesLoaded(false);
        return -1;
    }

    int ret;
    int64_t pts = -1;
    while ((ret = av_read_frame(formatContext, tempPacket)) >= 0) {
        if (tempPacket->stream_index == videoStreamIndex) {
            int retFlag;
            pts = currentFrameIndex;
            FrameData* frameData = m_frameQueue->getTailFrame(pts);
            copyFrame(tempPacket, frameData, retFlag);
            if (retFlag == 2)
                break;
            return pts;
        }
    }

    if (ret < 0 && ret != AVERROR_EOF) {
        ErrorReporter::instance().report("Failed to read raw YUV frame", LogLevel::Error);
    }

    av_packet_free(&tempPacket);
    return pts;
}

int64_t VideoDecoder::loadCompressedFrame() {
    // Allocate packet for decoding operation
    AVPacket* tempPacket = av_packet_alloc();
    if (!tempPacket) {
        ErrorReporter::instance().report("Could not allocate packet", LogLevel::Error);
        emit framesLoaded(false);
        return -1;
    }

    int ret;
    int64_t normalized_pts = -1;
    bool eof_reached = false;
    // Set the target pixel format for output
    AVPixelFormat dstFormat = AV_PIX_FMT_YUV420P;
    // Read frames until we get a video frame
    while ((ret = av_read_frame(formatContext, tempPacket)) >= 0 || !eof_reached) {
        if (ret < 0) {
            // EOF reached, send NULL packet to flush decoder
            eof_reached = true;
            ret = avcodec_send_packet(codecContext, nullptr);
        } else if (tempPacket->stream_index == videoStreamIndex) {
            // Send packet to decoder
            ret = avcodec_send_packet(codecContext, tempPacket);
            if (ret < 0) {
                ErrorReporter::instance().report("Failed to send packet to decoder", LogLevel::Error);
                av_packet_unref(tempPacket);
                break;
            }
        } else {
            av_packet_unref(tempPacket);
            continue;
        }

        // Try to receive a frame from the decoder
        while (true) {
            // Allocate a temporary frame for decoding
            AVFrame* tempFrame = av_frame_alloc();
            if (!tempFrame) {
                ErrorReporter::instance().report("Could not allocate temporary frame", LogLevel::Error);
                if (!eof_reached)
                    av_packet_unref(tempPacket);
                av_packet_free(&tempPacket);
                return -1;
            }

            ret = avcodec_receive_frame(codecContext, tempFrame);

            if (ret == 0) {
                int64_t raw_pts = tempFrame->pts;

                // Normalize PTS to frame number
                normalized_pts = raw_pts;
                if (m_needsTimebaseConversion && raw_pts != AV_NOPTS_VALUE) {
                    AVStream* videoStream = formatContext->streams[videoStreamIndex];
                    double frame_time = av_q2d(videoStream->time_base) * raw_pts;
                    normalized_pts = llrint(frame_time * m_framerate);
                }

                if (m_ptsOffset == -1 && normalized_pts >= 0) {
                    m_ptsOffset = normalized_pts;
                }

                normalized_pts -= m_ptsOffset;

                FrameData* frameData = m_frameQueue->getTailFrame(normalized_pts);

                int width = metadata.yWidth();
                int height = metadata.yHeight();

                // Always use target format descriptor for output
                const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(dstFormat);
                int uvWidth = AV_CEIL_RSHIFT(width, pixDesc->log2_chroma_w);

                uint8_t* dstData[4] = {frameData->yPtr(), frameData->uPtr(), frameData->vPtr(), nullptr};
                int dstLinesize[4] = {width, uvWidth, uvWidth, 0};

                // Hardware frame transfer if needed
                AVFrame* outputFrame = nullptr;
                if (hw_device_ctx && tempFrame->format == hw_pix_fmt) {
                    outputFrame = av_frame_alloc();
                    if (!outputFrame) {
                        ErrorReporter::instance().report("Could not allocate output frame", LogLevel::Error);
                        av_frame_free(&tempFrame);
                        av_packet_unref(tempPacket);
                        break;
                    }
                    if (av_hwframe_transfer_data(outputFrame, tempFrame, 0) < 0) {
                        ErrorReporter::instance().report("Failed to transfer frame from GPU to CPU", LogLevel::Error);
                        av_frame_free(&outputFrame);
                        av_frame_free(&tempFrame);
                        av_packet_unref(tempPacket);
                        break;
                    }
                } else {
                    outputFrame = tempFrame;
                }

                SwsContext* swsCtx = sws_getContext(outputFrame->width,
                                                    outputFrame->height,
                                                    (AVPixelFormat)outputFrame->format,
                                                    width,
                                                    height,
                                                    dstFormat,
                                                    SWS_BILINEAR,
                                                    nullptr,
                                                    nullptr,
                                                    nullptr);

                if (swsCtx) {
                    sws_scale(swsCtx,
                              (const uint8_t* const*)outputFrame->data,
                              outputFrame->linesize,
                              0,
                              outputFrame->height,
                              dstData,
                              dstLinesize);
                    sws_freeContext(swsCtx);

                    // Set pts to normalized pts
                    frameData->setPts(normalized_pts);
                    frameData->setEndFrame(false);
                    currentFrameIndex = normalized_pts;

                    // qDebug() << "VideoDecoder::loadCompressedFrame loaded frame" << normalized_pts << "from raw PTS"
                    //          << raw_pts << "at queue index" << normalized_pts;

                    av_frame_free(&tempFrame);
                    if (!eof_reached)
                        av_packet_unref(tempPacket);
                    av_packet_free(&tempPacket);
                    return normalized_pts;
                } else {
                    ErrorReporter::instance().report("Failed to create swsContext for YUV conversion", LogLevel::Error);
                    emit framesLoaded(false);
                    if (outputFrame != tempFrame)
                        av_frame_free(&outputFrame);
                    // Update metadata pixel format to dstFormat
                    metadata.setPixelFormat(dstFormat);
                    setFormat(dstFormat);
                    if (!eof_reached)
                        av_packet_unref(tempPacket);
                    av_packet_free(&tempPacket);
                    return -1;
                }
            } else if (ret == AVERROR(EAGAIN)) {
                // Need more input, break inner loop to read another packet
                av_frame_free(&tempFrame);
                break;
            } else if (ret == AVERROR_EOF) {
                // No more frames available
                av_frame_free(&tempFrame);
                av_packet_free(&tempPacket);
                return -1;
            } else {
                // Other error
                av_frame_free(&tempFrame);
                break;
            }
        }

        if (!eof_reached) {
            av_packet_unref(tempPacket);
        }
    }

    if (ret < 0 && ret != AVERROR_EOF) {
        ErrorReporter::instance().report("Failed to read frame", LogLevel::Error);
    }

    av_packet_free(&tempPacket);
    return -1;
}

/**
 * @brief Copies the decoded frame data into the provided FrameData structure.
 *
 * This function handles the copying of raw YUV frame data while preserving the original format
 * and populates the FrameData structure with the Y, U, and V plane pointers.
 *
 * @param tempPacket Pointer to the AVPacket containing the decoded frame data.
 * @param frameData Pointer to the FrameData structure to populate.
 * @param retFlag Reference to an integer flag indicating success or failure of the operation.
 */
void VideoDecoder::copyFrame(AVPacket*& tempPacket, FrameData* frameData, int& retFlag) {
    retFlag = 1;
    uint8_t* packetData = tempPacket->data;

    AVPixelFormat srcFmt = codecContext->pix_fmt;
    int width = metadata.yWidth();
    int height = metadata.yHeight();

    const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(srcFmt);
    if (!pixDesc) {
        ErrorReporter::instance().report("Failed to get pixel format descriptor", LogLevel::Error);
        av_packet_unref(tempPacket);
        retFlag = 2;
        return;
    }

    int uvWidth = AV_CEIL_RSHIFT(width, pixDesc->log2_chroma_w);
    int uvHeight = AV_CEIL_RSHIFT(height, pixDesc->log2_chroma_h);

    uint8_t* srcData[4] = {nullptr};
    int srcLinesize[4] = {};
    av_image_fill_arrays(srcData, srcLinesize, packetData, srcFmt, width, height, 1);
    uint8_t* dstData[4] = {frameData->yPtr(), frameData->uPtr(), frameData->vPtr(), nullptr};

    memcpy(dstData[0], srcData[0], width * height);
    memcpy(dstData[1], srcData[1], uvWidth * uvHeight);
    memcpy(dstData[2], srcData[2], uvWidth * uvHeight);

    frameData->setPts(currentFrameIndex);

    if (isYUV(codecContext->codec_id)) {
        if (currentFrameIndex == yuvTotalFrames - 1) {
            // qDebug() << "VideoDecoder::" << currentFrameIndex << "is end frame";
            frameData->setEndFrame(true);
            m_hitEndFrame = true;
        } else if (frameData->isEndFrame()) {
            // qDebug() << "VideoDecoder::" << currentFrameIndex << "is not end frame";
            frameData->setEndFrame(false);
        }
    }

    currentFrameIndex++;

    av_packet_unref(tempPacket);
    av_packet_free(&tempPacket);
}

int VideoDecoder::getTotalFrames() {
    if (isYUV(codecContext->codec_id) && yuvTotalFrames > 0) {
        return yuvTotalFrames;
    }

    if (!formatContext || videoStreamIndex < 0) {
        return -1;
    }

    AVStream* videoStream = formatContext->streams[videoStreamIndex];

    if (videoStream->nb_frames > 0) {
        return static_cast<int>(videoStream->nb_frames);
    }

    return -1;
}

int64_t VideoDecoder::getDurationMs() {
    if (!formatContext || videoStreamIndex < 0) {
        return -1;
    }

    AVStream* videoStream = formatContext->streams[videoStreamIndex];
    if (videoStream->duration != AV_NOPTS_VALUE) {
        return av_rescale_q(videoStream->duration, videoStream->time_base, AVRational{1, 1000});
    }

    return -1;
}

void VideoDecoder::seekTo(int64_t targetPts) {
    if (!formatContext || !codecContext || videoStreamIndex < 0) {
        ErrorReporter::instance().report("VideoDecoder not properly initialized for seeking", LogLevel::Error);
        return;
    }

    if (targetPts < 0) {
        qDebug() << "Decoder:: internal seek asked for negative pts: " << targetPts;
        targetPts = 0;
    }

    // Special handling for YUV files
    if (isYUV(codecContext->codec_id)) {
        // For YUV files, we need to seek directly to the byte position
        if (targetPts >= yuvTotalFrames) {
            targetPts = yuvTotalFrames - 1;
        }

        // Calculate frame size
        const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(codecContext->pix_fmt);
        int uvWidth = AV_CEIL_RSHIFT(m_width, pixDesc->log2_chroma_w);
        int uvHeight = AV_CEIL_RSHIFT(m_height, pixDesc->log2_chroma_h);
        int ySize = m_width * m_height;
        int uvSize = uvWidth * uvHeight;
        int frameSize = ySize + 2 * uvSize;

        // Calculate byte position for target frame
        int64_t bytePosition = targetPts * frameSize;

        // Seek to the calculated position
        int ret = avio_seek(formatContext->pb, bytePosition, SEEK_SET);
        if (ret < 0) {
            ErrorReporter::instance().report("Failed to seek in YUV file to frame: " + std::to_string(targetPts),
                                             LogLevel::Error);
            return;
        }

        // qDebug() << "Decoder::seekTo YUV file to frame" << targetPts << "at byte position" << bytePosition;
        currentFrameIndex = targetPts;
        return;
    }

    int64_t seek_timestamp = targetPts;
    if (m_needsTimebaseConversion) {
        AVStream* videoStream = formatContext->streams[videoStreamIndex];
        double timestamp_seconds = targetPts / m_framerate;
        seek_timestamp = llrint(timestamp_seconds / av_q2d(videoStream->time_base));

        // qDebug() << "Decoder::seekTo frame" << targetPts << "-> time" << timestamp_seconds << "s -> stream_ts"
        //          << seek_timestamp;
    }

    int ret = av_seek_frame(formatContext, videoStreamIndex, seek_timestamp, AVSEEK_FLAG_BACKWARD);

    if (ret < 0) {
        ErrorReporter::instance().report("Failed to seek to timestamp: " + std::to_string(targetPts), LogLevel::Error);
        return;
    }

    avcodec_flush_buffers(codecContext);

    int64_t current_pts = -1;
    while (current_pts < targetPts - 1) {
        // Decode frames until we reach the exact target PTS
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        if (!packet || !frame) {
            ErrorReporter::instance().report("Failed to allocate packet or frame for seeking", LogLevel::Error);
            if (packet)
                av_packet_free(&packet);
            if (frame)
                av_frame_free(&frame);
            return;
        }

        int ret = av_read_frame(formatContext, packet);
        if (ret < 0) {
            qDebug() << "Decoder::seekTo reached EOF while seeking to frame" << targetPts;
            break;
        }

        if (packet->stream_index == videoStreamIndex) {
            ret = avcodec_send_packet(codecContext, packet);
            if (ret < 0) {
                qDebug() << "Decoder::seekTo failed to send packet to decoder";
                av_packet_unref(packet);
                continue;
            }

            ret = avcodec_receive_frame(codecContext, frame);
            if (ret == 0) {
                current_pts = frame->pts;
                if (m_needsTimebaseConversion && current_pts != AV_NOPTS_VALUE) {
                    AVStream* videoStream = formatContext->streams[videoStreamIndex];
                    AVRational targetTimebase = av_d2q(1.0 / m_framerate, 1000000);
                    current_pts = av_rescale_q(current_pts, videoStream->time_base, targetTimebase);
                }
                // qDebug() << "Decoder::seekTo decoded frame with PTS:" << current_pts << "target:" << targetPts;
            }
        }
        av_packet_unref(packet);
        av_packet_free(&packet);
        av_frame_free(&frame);
    }

    currentFrameIndex = targetPts;
}

void VideoDecoder::seek(int64_t targetPts) {
    // Load past & future frames around the target PTS
    int64_t startPts = std::max(targetPts - m_frameQueue->getSize() / 4, int64_t{0});
    seekTo(startPts);
    qDebug() << "Decoder::Seeking to currentFrameIndex: " << currentFrameIndex;
    loadFrames(m_frameQueue->getSize() / 2);
    qDebug() << "Decoder::Loaded until currentFrameIndex: " << currentFrameIndex;
    emit frameSeeked(targetPts);
}

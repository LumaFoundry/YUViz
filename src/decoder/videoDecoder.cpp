#include "videoDecoder.h"
#include <QFile>
#include <chrono>
#include <thread>
#include "utils/debugManager.h"
#include "utils/videoFormatUtils.h"

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

void VideoDecoder::setForceSoftwareDecoding(bool force) {
    m_forceSoftwareDecoding = force;
    if (force) {
        debug("vd", "Software decoding enforced - hardware acceleration disabled");
    }
}

/**
 * @brief Opens a video file for decoding and initializes FrameMeta object.
 */
void VideoDecoder::openFile() {
    closeFile();

    const AVInputFormat* inputFormat = nullptr;

    QString qFileName = QString::fromStdString(m_fileName);
    QString formatIdentifier = VideoFormatUtils::detectFormatFromExtension(qFileName);

    // Check if it's Y4M format
    if (VideoFormatUtils::getFormatType(formatIdentifier) == FormatType::Y4M) {
        m_isY4M = true;
        m_y4mInfo = Y4MParser::parseHeader(qFileName);

        if (!m_y4mInfo.isValid) {
            ErrorReporter::instance().report("Y4M file header parsing failed", LogLevel::Error);
            return;
        }

        // Set decoder parameters from Y4M header information
        setDimensions(m_y4mInfo.width, m_y4mInfo.height);
        setFramerate(m_y4mInfo.frameRate);
        setFormat(m_y4mInfo.pixelFormat);

        debug("vd",
              QString("Detected Y4M file, resolution: %1x%2, frame rate: %3, pixel format: %4")
                  .arg(m_y4mInfo.width)
                  .arg(m_y4mInfo.height)
                  .arg(m_y4mInfo.frameRate)
                  .arg(av_get_pix_fmt_name(m_y4mInfo.pixelFormat)));

        // Y4M files use special processing, no need to open through FFmpeg
        // Set metadata directly and return
        const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(m_y4mInfo.pixelFormat);
        int uvWidth = AV_CEIL_RSHIFT(m_y4mInfo.width, pixDesc->log2_chroma_w);
        int uvHeight = AV_CEIL_RSHIFT(m_y4mInfo.height, pixDesc->log2_chroma_h);

        metadata.setYWidth(m_y4mInfo.width);
        metadata.setYHeight(m_y4mInfo.height);
        metadata.setUVWidth(uvWidth);
        metadata.setUVHeight(uvHeight);
        metadata.setPixelFormat(m_y4mInfo.pixelFormat);
        metadata.setTimeBase({1, static_cast<int>(m_y4mInfo.frameRate)});
        metadata.setSampleAspectRatio({1, 1});
        metadata.setColorRange(AVCOL_RANGE_UNSPECIFIED);
        metadata.setColorSpace(AVCOL_SPC_UNSPECIFIED);
        metadata.setFilename(m_fileName);
        metadata.setCodecName("Y4M");

        int totalFrames = Y4MParser::calculateTotalFrames(qFileName, m_y4mInfo);
        metadata.setTotalFrames(totalFrames);
        yuvTotalFrames = totalFrames;

        debug("vd", QString("Y4M file total frames: %1").arg(totalFrames));

        currentFrameIndex = 0;
        return;
    } else if (VideoFormatUtils::getFormatType(formatIdentifier) == FormatType::RAW_YUV) {
        m_isY4M = false;
        inputFormat = av_find_input_format("rawvideo");
        debug("vd", "Detected raw YUV file, using rawvideo input format");
    } else {
        m_isY4M = false;
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
        debug("vd", QString("✗ No decoder found for codec: %1").arg(avcodec_get_name(codecId)));
        return;
    }

    debug("vd", QString("Found decoder: %1 for codec: %2").arg(codec->name).arg(avcodec_get_name(codecId)));

    // Try to enable hardware acceleration (only for compressed formats)
    if (m_forceSoftwareDecoding) {
        debug("vd", "Software decoding forced - skipping hardware acceleration");
    } else if (isYUV(codecId)) {
        debug("vd", "RAW/YUV format detected - hardware acceleration not applicable");
    } else {
        // Only try hardware acceleration for compressed formats
#if defined(Q_OS_MACOS)
        if (initializeHardwareDecoder(AV_HWDEVICE_TYPE_VIDEOTOOLBOX, AV_PIX_FMT_VIDEOTOOLBOX)) {
            debug("vd", QString("Hardware acceleration enabled: VideoToolbox for %1").arg(codec->name));
        } else {
            debug("vd", "VideoToolbox hardware acceleration not available, using software decoding");
        }
#elif defined(Q_OS_LINUX)
        if (initializeHardwareDecoder(AV_HWDEVICE_TYPE_VAAPI, AV_PIX_FMT_VAAPI)) {
            debug("vd", QString("Hardware acceleration enabled: VAAPI for %1").arg(codec->name));
        } else {
            debug("vd", "VAAPI hardware acceleration not available, using software decoding");
        }
#elif defined(Q_OS_WIN)
        if (initializeHardwareDecoder(AV_HWDEVICE_TYPE_D3D11VA, AV_PIX_FMT_D3D11)) {
            debug("vd", QString("Hardware acceleration enabled: D3D11VA for %1").arg(codec->name));
        } else {
            debug("vd", "D3D11VA hardware acceleration not available, using software decoding");
        }
#else
        debug("vd", "No hardware acceleration available on this platform");
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
        setDimensions(codecContext->width, codecContext->height);
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
        debug("vd", QString("Final status: RAW format processing - %1 (no decoding required)").arg(codec->name));
    } else if (isActuallyUsingHardware) {
#if defined(Q_OS_MACOS)
        debug("vd", QString("Final status: Hardware decoding active - %1 with VideoToolbox").arg(codec->name));
#elif defined(Q_OS_LINUX)
        debug("vd", QString("Final status: Hardware decoding active - %1 with VAAPI").arg(codec->name));
#elif defined(Q_OS_WIN)
        debug("vd", QString("Final status: Hardware decoding active - %1 with D3D11VA").arg(codec->name));
#endif
    } else {
        debug("vd", QString("Final status: Software decoding active - %1").arg(codec->name));
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
    metadata.setTimeBase(videoStream->time_base);
    metadata.setSampleAspectRatio(videoStream->sample_aspect_ratio);
    metadata.setColorRange(codecContext->color_range);
    metadata.setColorSpace(codecContext->colorspace);
    metadata.setFilename(m_fileName);
    metadata.setCodecName(codec->name ? std::string(codec->name) : "Unknown");
    metadata.setDuration(getDurationMs());
    metadata.setTotalFrames(getTotalFrames());
    setFormat(codecContext->pix_fmt);

    if (isYUV(codecContext->codec_id)) {
        int frameSize = calculateFrameSize(codecContext->pix_fmt, m_width, m_height);

        QFileInfo info(QString::fromStdString(m_fileName));
        int64_t fileSize = info.size();
        yuvTotalFrames = fileSize / frameSize;
        metadata.setTotalFrames(yuvTotalFrames);
    } else {
        AVRational timeBase = videoStream->time_base;
        AVRational frameRate = videoStream->avg_frame_rate;
        metadata.setTotalFrames(getTotalFrames());
        setFramerate(frameRate.num);

        if (frameRate.num == 0 && timeBase.den == 0) {
            metadata.setTimeBase({1, 25});
            setFramerate(25);
        } else if (frameRate.num == 0) {
            setFramerate(timeBase.den);
        } else if (videoStream->time_base.den == 0) {
            metadata.setTimeBase({frameRate.den, frameRate.num});
        }

        if (timeBase.den >= 1000) {
            m_needsTimebaseConversion = true;
            metadata.setTimeBase(av_d2q(1.0 / m_framerate, 1000000));
        }
    }

    debug("vd", QString("Timebase: %1/%2").arg(metadata.timeBase().num).arg(metadata.timeBase().den));
    debug("vd", QString("Framerate: %1").arg(m_framerate));

    currentFrameIndex = 0;
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

    // Y4M files have special processing logic
    if (m_isY4M) {
        // Y4M file processing logic
    } else if (!formatContext || !codecContext) {
        ErrorReporter::instance().report("VideoDecoder not properly initialized", LogLevel::Error);
        emit framesLoaded(false);
        return;
    }

    bool isRawYUV = !m_isY4M && isYUV(codecContext->codec_id);
    int64_t maxpts = -1;
    int64_t minpts = INT64_MAX;

    debug("vd",
          QString("loadFrames called with num_frames: %1, direction: %2, currentFrameIndex: %3")
              .arg(num_frames)
              .arg(direction)
              .arg(currentFrameIndex));

    // Seek to correct frame before loading
    if (direction == -1) {
        if (currentFrameIndex == 0) {
            debug("vd", "At the beginning of the video, cannot seek backward");
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
        debug("vd", QString("seeking to %1").arg(currentFrameIndex));
        // Make sure we don't load more than half of the queue size
        num_frames = std::min(num_frames, m_frameQueue->getSize() / 2);
    }

    localTail = currentFrameIndex;

    for (int i = 0; i < num_frames; ++i) {
        int64_t temp_pts;
        if (m_isY4M) {
            temp_pts = loadY4MFrame();
        } else if (isRawYUV) {
            temp_pts = loadYUVFrame();
        } else {
            temp_pts = loadCompressedFrame();
            debug("vd", QString("loadCompressedFrame returned pts: %1").arg(temp_pts));
        }

        // EOF
        if (temp_pts == -1) {
            debug("vd", "Reached EOF, marking last frame as end frame");
            if (currentFrameIndex > 0) {
                int totalFrames = getTotalFrames();
                int64_t lastFramePts = currentFrameIndex - 1;

                if (totalFrames > 0 && lastFramePts < totalFrames - 1) {
                    lastFramePts = totalFrames - 1;
                }

                FrameData* lastFrame = m_frameQueue->getTailFrame(lastFramePts);
                if (lastFrame) {
                    lastFrame->setEndFrame(true);
                    debug(
                        "vd",
                        QString("Marked frame %1 as end frame (total frames: %2)").arg(lastFramePts).arg(totalFrames));
                }
            }
            break;
        }

        if (!isRawYUV && !m_isY4M) {
            int totalFrames = getTotalFrames();
            if (totalFrames > 0 && temp_pts >= totalFrames - 1) {
                FrameData* endFrame = m_frameQueue->getTailFrame(temp_pts);
                if (endFrame) {
                    endFrame->setEndFrame(true);
                    debug("vd",
                          QString("Marked frame %1 as end frame (reached total frames: %2)")
                              .arg(temp_pts)
                              .arg(totalFrames));
                }
            }
        }

        maxpts = std::max(maxpts, temp_pts);
        minpts = std::min(minpts, std::max(temp_pts, (int64_t)0));
    }

    debug("vd", QString("Loaded from %1 to %2 in direction %3").arg(localTail).arg(currentFrameIndex).arg(direction));

    if (direction == 1) {
        m_frameQueue->updateTail(maxpts);
    } else {
        m_frameQueue->updateTail(maxpts);
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

bool VideoDecoder::isPackedYUV(AVPixelFormat pixFmt) {
    switch (pixFmt) {
    case AV_PIX_FMT_YUYV422: // YUYV (YUY2)
    case AV_PIX_FMT_UYVY422: // UYVY
        return true;
    default:
        return false;
    }
}

bool VideoDecoder::isSemiPlanarYUV(AVPixelFormat pixFmt) {
    switch (pixFmt) {
    case AV_PIX_FMT_NV12: // NV12: Y plane + UV interleaved plane
    case AV_PIX_FMT_NV21: // NV21: Y plane + VU interleaved plane
        return true;
    default:
        return false;
    }
}

int VideoDecoder::calculateFrameSize(AVPixelFormat pixFmt, int width, int height) {
    // For packed YUV formats
    if (isPackedYUV(pixFmt)) {
        // All packed YUV 4:2:2 formats use 2 bytes per pixel
        return width * height * 2;
    }

    // For semi-planar YUV formats (NV12/NV21)
    if (isSemiPlanarYUV(pixFmt)) {
        // Y plane + UV interleaved plane (4:2:0 subsampling)
        return width * height + width * height / 2;
    }

    // For planar YUV formats, use FFmpeg's pixel format descriptor
    const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(pixFmt);
    if (!pixDesc) {
        // Default to YUV420P if format is unknown
        return width * height + 2 * ((width + 1) / 2) * ((height + 1) / 2);
    }

    int ySize = width * height;
    int uvWidth = AV_CEIL_RSHIFT(width, pixDesc->log2_chroma_w);
    int uvHeight = AV_CEIL_RSHIFT(height, pixDesc->log2_chroma_h);
    int uvSize = uvWidth * uvHeight;

    return ySize + 2 * uvSize;
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
    debug("vd", QString("Successfully initialized hardware decoder: %1").arg(av_hwdevice_get_type_name(deviceType)));
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
            av_packet_free(&tempPacket);
            return pts;
        }
        av_packet_unref(tempPacket);
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
    AVPixelFormat dstFormat = AV_PIX_FMT_YUV420P;

    while ((ret = av_read_frame(formatContext, tempPacket)) >= 0 || !eof_reached) {
        if (ret < 0) {
            eof_reached = true;
            ret = avcodec_send_packet(codecContext, nullptr);
        } else if (tempPacket->stream_index == videoStreamIndex) {
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

        while (true) {
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

                    if (outputFrame != tempFrame) {
                        av_frame_free(&outputFrame);
                    }

                    // Set pts to normalized pts
                    frameData->setPts(normalized_pts);
                    frameData->setEndFrame(false);
                    currentFrameIndex = normalized_pts + 1;

                    debug("vd",
                          QString("loadCompressedFrame loaded frame %1 from raw PTS %2 at queue index %3")
                              .arg(normalized_pts)
                              .arg(raw_pts)
                              .arg(normalized_pts));

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
                    av_frame_free(&tempFrame);
                    metadata.setPixelFormat(dstFormat);
                    setFormat(dstFormat);
                    if (!eof_reached)
                        av_packet_unref(tempPacket);
                    av_packet_free(&tempPacket);
                    return -1;
                }
            } else if (ret == AVERROR(EAGAIN)) {
                av_frame_free(&tempFrame);
                break;
            } else if (ret == AVERROR_EOF) {
                av_frame_free(&tempFrame);
                av_packet_free(&tempPacket);
                return -1;
            } else {
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
        retFlag = 2;
        return;
    }

    // Handle packed YUV formats differently
    if (isPackedYUV(srcFmt)) {
        debug("vd",
              QString("Processing packed YUV format: %1 Dimensions: %2x%3 Packet size: %4")
                  .arg(av_get_pix_fmt_name(srcFmt))
                  .arg(width)
                  .arg(height)
                  .arg(tempPacket->size));

        uint8_t* yPtr = frameData->yPtr();
        uint8_t* uPtr = frameData->uPtr();
        uint8_t* vPtr = frameData->vPtr();

        // Verify pointers are valid
        if (!yPtr || !uPtr || !vPtr) {
            ErrorReporter::instance().report("Invalid frame data pointers", LogLevel::Error);
            retFlag = 2;
            return;
        }

        // Convert packed YUV to planar YUV422P for internal processing (optimized)
        if (srcFmt == AV_PIX_FMT_UYVY422) {
            // UYVY: U0 Y0 V0 Y1 (4 bytes for 2 pixels)
            debug("vd", "Converting UYVY422 to YUV422P...");

            const int uvWidth = width / 2;

            // Optimized conversion with better memory access patterns
            for (int y = 0; y < height; y++) {
                const uint8_t* srcLine = packetData + y * width * 2;
                uint8_t* yLine = yPtr + y * width;
                uint8_t* uLine = uPtr + y * uvWidth;
                uint8_t* vLine = vPtr + y * uvWidth;

                // Process 16 pixels at a time for better cache locality and reduced loop overhead
                int x = 0;
                for (; x <= width - 16; x += 16) {
                    const uint8_t* src = srcLine + x * 2;

                    // Extract Y values (16 pixels) - unrolled for better performance
                    yLine[x] = src[1];       // Y0
                    yLine[x + 1] = src[3];   // Y1
                    yLine[x + 2] = src[5];   // Y2
                    yLine[x + 3] = src[7];   // Y3
                    yLine[x + 4] = src[9];   // Y4
                    yLine[x + 5] = src[11];  // Y5
                    yLine[x + 6] = src[13];  // Y6
                    yLine[x + 7] = src[15];  // Y7
                    yLine[x + 8] = src[17];  // Y8
                    yLine[x + 9] = src[19];  // Y9
                    yLine[x + 10] = src[21]; // Y10
                    yLine[x + 11] = src[23]; // Y11
                    yLine[x + 12] = src[25]; // Y12
                    yLine[x + 13] = src[27]; // Y13
                    yLine[x + 14] = src[29]; // Y14
                    yLine[x + 15] = src[31]; // Y15

                    // Extract U values (8 UV pairs)
                    uLine[x / 2] = src[0];      // U0
                    uLine[x / 2 + 1] = src[4];  // U1
                    uLine[x / 2 + 2] = src[8];  // U2
                    uLine[x / 2 + 3] = src[12]; // U3
                    uLine[x / 2 + 4] = src[16]; // U4
                    uLine[x / 2 + 5] = src[20]; // U5
                    uLine[x / 2 + 6] = src[24]; // U6
                    uLine[x / 2 + 7] = src[28]; // U7

                    // Extract V values (8 UV pairs)
                    vLine[x / 2] = src[2];      // V0
                    vLine[x / 2 + 1] = src[6];  // V1
                    vLine[x / 2 + 2] = src[10]; // V2
                    vLine[x / 2 + 3] = src[14]; // V3
                    vLine[x / 2 + 4] = src[18]; // V4
                    vLine[x / 2 + 5] = src[22]; // V5
                    vLine[x / 2 + 6] = src[26]; // V6
                    vLine[x / 2 + 7] = src[30]; // V7
                }

                // Handle remaining pixels in 8-pixel chunks
                for (; x <= width - 8; x += 8) {
                    const uint8_t* src = srcLine + x * 2;

                    // Extract Y values (8 pixels)
                    yLine[x] = src[1];      // Y0
                    yLine[x + 1] = src[3];  // Y1
                    yLine[x + 2] = src[5];  // Y2
                    yLine[x + 3] = src[7];  // Y3
                    yLine[x + 4] = src[9];  // Y4
                    yLine[x + 5] = src[11]; // Y5
                    yLine[x + 6] = src[13]; // Y6
                    yLine[x + 7] = src[15]; // Y7

                    // Extract U values (4 UV pairs)
                    uLine[x / 2] = src[0];      // U0
                    uLine[x / 2 + 1] = src[4];  // U1
                    uLine[x / 2 + 2] = src[8];  // U2
                    uLine[x / 2 + 3] = src[12]; // U3

                    // Extract V values (4 UV pairs)
                    vLine[x / 2] = src[2];      // V0
                    vLine[x / 2 + 1] = src[6];  // V1
                    vLine[x / 2 + 2] = src[10]; // V2
                    vLine[x / 2 + 3] = src[14]; // V3
                }

                // Handle remaining pixels
                for (; x < width; x += 2) {
                    const uint8_t* src = srcLine + x * 2;
                    yLine[x] = src[1]; // Y0
                    if (x + 1 < width) {
                        yLine[x + 1] = src[3]; // Y1
                    }
                    uLine[x / 2] = src[0]; // U0
                    vLine[x / 2] = src[2]; // V0
                }
            }

        } else if (srcFmt == AV_PIX_FMT_YUYV422) {
            // YUYV: Y0 U0 Y1 V0 (4 bytes for 2 pixels)
            debug("vd", "Converting YUYV422 to YUV422P...");

            const int uvWidth = width / 2;

            // Optimized conversion with better memory access patterns
            for (int y = 0; y < height; y++) {
                const uint8_t* srcLine = packetData + y * width * 2;
                uint8_t* yLine = yPtr + y * width;
                uint8_t* uLine = uPtr + y * uvWidth;
                uint8_t* vLine = vPtr + y * uvWidth;

                int x = 0;
                for (; x <= width - 16; x += 16) {
                    const uint8_t* src = srcLine + x * 2;

                    // Extract Y values (16 pixels) - unrolled for better performance
                    yLine[x] = src[0];       // Y0
                    yLine[x + 1] = src[2];   // Y1
                    yLine[x + 2] = src[4];   // Y2
                    yLine[x + 3] = src[6];   // Y3
                    yLine[x + 4] = src[8];   // Y4
                    yLine[x + 5] = src[10];  // Y5
                    yLine[x + 6] = src[12];  // Y6
                    yLine[x + 7] = src[14];  // Y7
                    yLine[x + 8] = src[16];  // Y8
                    yLine[x + 9] = src[18];  // Y9
                    yLine[x + 10] = src[20]; // Y10
                    yLine[x + 11] = src[22]; // Y11
                    yLine[x + 12] = src[24]; // Y12
                    yLine[x + 13] = src[26]; // Y13
                    yLine[x + 14] = src[28]; // Y14
                    yLine[x + 15] = src[30]; // Y15

                    // Extract U values (8 UV pairs)
                    uLine[x / 2] = src[1];      // U0
                    uLine[x / 2 + 1] = src[5];  // U1
                    uLine[x / 2 + 2] = src[9];  // U2
                    uLine[x / 2 + 3] = src[13]; // U3
                    uLine[x / 2 + 4] = src[17]; // U4
                    uLine[x / 2 + 5] = src[21]; // U5
                    uLine[x / 2 + 6] = src[25]; // U6
                    uLine[x / 2 + 7] = src[29]; // U7

                    // Extract V values (8 UV pairs)
                    vLine[x / 2] = src[3];      // V0
                    vLine[x / 2 + 1] = src[7];  // V1
                    vLine[x / 2 + 2] = src[11]; // V2
                    vLine[x / 2 + 3] = src[15]; // V3
                    vLine[x / 2 + 4] = src[19]; // V4
                    vLine[x / 2 + 5] = src[23]; // V5
                    vLine[x / 2 + 6] = src[27]; // V6
                    vLine[x / 2 + 7] = src[31]; // V7
                }

                // Handle remaining pixels in 8-pixel chunks
                for (; x <= width - 8; x += 8) {
                    const uint8_t* src = srcLine + x * 2;

                    // Extract Y values (8 pixels)
                    yLine[x] = src[0];      // Y0
                    yLine[x + 1] = src[2];  // Y1
                    yLine[x + 2] = src[4];  // Y2
                    yLine[x + 3] = src[6];  // Y3
                    yLine[x + 4] = src[8];  // Y4
                    yLine[x + 5] = src[10]; // Y5
                    yLine[x + 6] = src[12]; // Y6
                    yLine[x + 7] = src[14]; // Y7

                    // Extract U values (4 UV pairs)
                    uLine[x / 2] = src[1];      // U0
                    uLine[x / 2 + 1] = src[5];  // U1
                    uLine[x / 2 + 2] = src[9];  // U2
                    uLine[x / 2 + 3] = src[13]; // U3

                    // Extract V values (4 UV pairs)
                    vLine[x / 2] = src[3];      // V0
                    vLine[x / 2 + 1] = src[7];  // V1
                    vLine[x / 2 + 2] = src[11]; // V2
                    vLine[x / 2 + 3] = src[15]; // V3
                }

                // Handle remaining pixels
                for (; x < width; x += 2) {
                    const uint8_t* src = srcLine + x * 2;
                    yLine[x] = src[0]; // Y0
                    if (x + 1 < width) {
                        yLine[x + 1] = src[2]; // Y1
                    }
                    uLine[x / 2] = src[1]; // U0
                    vLine[x / 2] = src[3]; // V0
                }
            }
        }

        // Update metadata to reflect the converted planar format
        if (srcFmt == AV_PIX_FMT_UYVY422 || srcFmt == AV_PIX_FMT_YUYV422) {
            metadata.setPixelFormat(AV_PIX_FMT_YUV422P);
            setFormat(AV_PIX_FMT_YUV422P);
        }
    } else if (isSemiPlanarYUV(srcFmt)) {
        debug("vd",
              QString("Processing semi-planar YUV format: %1 Dimensions: %2x%3 Packet size: %4")
                  .arg(av_get_pix_fmt_name(srcFmt))
                  .arg(width)
                  .arg(height)
                  .arg(tempPacket->size));

        uint8_t* yPtr = frameData->yPtr();
        uint8_t* uPtr = frameData->uPtr();
        uint8_t* vPtr = frameData->vPtr();

        // Verify pointers are valid
        if (!yPtr || !uPtr || !vPtr) {
            ErrorReporter::instance().report("Invalid frame data pointers", LogLevel::Error);
            retFlag = 2;
            return;
        }

        // Copy Y plane (first plane)
        int ySize = width * height;
        memcpy(yPtr, packetData, ySize);

        // Process UV interleaved plane (second plane)
        uint8_t* uvData = packetData + ySize;
        int uvWidth = width / 2;
        int uvHeight = height / 2;

        if (srcFmt == AV_PIX_FMT_NV12) {
            // NV12: Y plane + UV interleaved plane
            debug("vd", "Converting NV12 to YUV420P...");

            // Optimized conversion with better memory access patterns
            for (int y = 0; y < uvHeight; y++) {
                const uint8_t* uvLine = uvData + y * width;
                uint8_t* uLine = uPtr + y * uvWidth;
                uint8_t* vLine = vPtr + y * uvWidth;

                int x = 0;
                for (; x <= uvWidth - 16; x += 16) {
                    const uint8_t* src = uvLine + x * 2;

                    // Extract U values (16 UV pairs) - unrolled for better performance
                    uLine[x] = src[0];       // U0
                    uLine[x + 1] = src[2];   // U1
                    uLine[x + 2] = src[4];   // U2
                    uLine[x + 3] = src[6];   // U3
                    uLine[x + 4] = src[8];   // U4
                    uLine[x + 5] = src[10];  // U5
                    uLine[x + 6] = src[12];  // U6
                    uLine[x + 7] = src[14];  // U7
                    uLine[x + 8] = src[16];  // U8
                    uLine[x + 9] = src[18];  // U9
                    uLine[x + 10] = src[20]; // U10
                    uLine[x + 11] = src[22]; // U11
                    uLine[x + 12] = src[24]; // U12
                    uLine[x + 13] = src[26]; // U13
                    uLine[x + 14] = src[28]; // U14
                    uLine[x + 15] = src[30]; // U15

                    // Extract V values (16 UV pairs)
                    vLine[x] = src[1];       // V0
                    vLine[x + 1] = src[3];   // V1
                    vLine[x + 2] = src[5];   // V2
                    vLine[x + 3] = src[7];   // V3
                    vLine[x + 4] = src[9];   // V4
                    vLine[x + 5] = src[11];  // V5
                    vLine[x + 6] = src[13];  // V6
                    vLine[x + 7] = src[15];  // V7
                    vLine[x + 8] = src[17];  // V8
                    vLine[x + 9] = src[19];  // V9
                    vLine[x + 10] = src[21]; // V10
                    vLine[x + 11] = src[23]; // V11
                    vLine[x + 12] = src[25]; // V12
                    vLine[x + 13] = src[27]; // V13
                    vLine[x + 14] = src[29]; // V14
                    vLine[x + 15] = src[31]; // V15
                }

                // Handle remaining pixels in 8-pixel chunks
                for (; x <= uvWidth - 8; x += 8) {
                    const uint8_t* src = uvLine + x * 2;

                    // Extract U values (8 UV pairs)
                    uLine[x] = src[0];      // U0
                    uLine[x + 1] = src[2];  // U1
                    uLine[x + 2] = src[4];  // U2
                    uLine[x + 3] = src[6];  // U3
                    uLine[x + 4] = src[8];  // U4
                    uLine[x + 5] = src[10]; // U5
                    uLine[x + 6] = src[12]; // U6
                    uLine[x + 7] = src[14]; // U7

                    // Extract V values (8 UV pairs)
                    vLine[x] = src[1];      // V0
                    vLine[x + 1] = src[3];  // V1
                    vLine[x + 2] = src[5];  // V2
                    vLine[x + 3] = src[7];  // V3
                    vLine[x + 4] = src[9];  // V4
                    vLine[x + 5] = src[11]; // V5
                    vLine[x + 6] = src[13]; // V6
                    vLine[x + 7] = src[15]; // V7
                }

                // Handle remaining UV pairs
                for (; x < uvWidth; x++) {
                    const uint8_t* src = uvLine + x * 2;
                    uLine[x] = src[0]; // U component
                    vLine[x] = src[1]; // V component
                }
            }
        } else if (srcFmt == AV_PIX_FMT_NV21) {
            // NV21: Y plane + VU interleaved plane
            debug("vd", "Converting NV21 to YUV420P...");

            // Optimized conversion with better memory access patterns
            for (int y = 0; y < uvHeight; y++) {
                const uint8_t* vuLine = uvData + y * width;
                uint8_t* uLine = uPtr + y * uvWidth;
                uint8_t* vLine = vPtr + y * uvWidth;

                int x = 0;
                for (; x <= uvWidth - 16; x += 16) {
                    const uint8_t* src = vuLine + x * 2;

                    // Extract V values (16 VU pairs) - unrolled for better performance
                    vLine[x] = src[0];       // V0
                    vLine[x + 1] = src[2];   // V1
                    vLine[x + 2] = src[4];   // V2
                    vLine[x + 3] = src[6];   // V3
                    vLine[x + 4] = src[8];   // V4
                    vLine[x + 5] = src[10];  // V5
                    vLine[x + 6] = src[12];  // V6
                    vLine[x + 7] = src[14];  // V7
                    vLine[x + 8] = src[16];  // V8
                    vLine[x + 9] = src[18];  // V9
                    vLine[x + 10] = src[20]; // V10
                    vLine[x + 11] = src[22]; // V11
                    vLine[x + 12] = src[24]; // V12
                    vLine[x + 13] = src[26]; // V13
                    vLine[x + 14] = src[28]; // V14
                    vLine[x + 15] = src[30]; // V15

                    // Extract U values (16 VU pairs)
                    uLine[x] = src[1];       // U0
                    uLine[x + 1] = src[3];   // U1
                    uLine[x + 2] = src[5];   // U2
                    uLine[x + 3] = src[7];   // U3
                    uLine[x + 4] = src[9];   // U4
                    uLine[x + 5] = src[11];  // U5
                    uLine[x + 6] = src[13];  // U6
                    uLine[x + 7] = src[15];  // U7
                    uLine[x + 8] = src[17];  // U8
                    uLine[x + 9] = src[19];  // U9
                    uLine[x + 10] = src[21]; // U10
                    uLine[x + 11] = src[23]; // U11
                    uLine[x + 12] = src[25]; // U12
                    uLine[x + 13] = src[27]; // U13
                    uLine[x + 14] = src[29]; // U14
                    uLine[x + 15] = src[31]; // U15
                }

                // Handle remaining pixels in 8-pixel chunks
                for (; x <= uvWidth - 8; x += 8) {
                    const uint8_t* src = vuLine + x * 2;

                    // Extract V values (8 VU pairs)
                    vLine[x] = src[0];      // V0
                    vLine[x + 1] = src[2];  // V1
                    vLine[x + 2] = src[4];  // V2
                    vLine[x + 3] = src[6];  // V3
                    vLine[x + 4] = src[8];  // V4
                    vLine[x + 5] = src[10]; // V5
                    vLine[x + 6] = src[12]; // V6
                    vLine[x + 7] = src[14]; // V7

                    // Extract U values (8 VU pairs)
                    uLine[x] = src[1];      // U0
                    uLine[x + 1] = src[3];  // U1
                    uLine[x + 2] = src[5];  // U2
                    uLine[x + 3] = src[7];  // U3
                    uLine[x + 4] = src[9];  // U4
                    uLine[x + 5] = src[11]; // U5
                    uLine[x + 6] = src[13]; // U6
                    uLine[x + 7] = src[15]; // U7
                }

                // Handle remaining VU pairs
                for (; x < uvWidth; x++) {
                    const uint8_t* src = vuLine + x * 2;
                    vLine[x] = src[0]; // V component
                    uLine[x] = src[1]; // U component
                }
            }
        }

        // Update metadata to reflect the converted planar format
        if (srcFmt == AV_PIX_FMT_NV12 || srcFmt == AV_PIX_FMT_NV21) {
            metadata.setPixelFormat(AV_PIX_FMT_YUV420P);
            setFormat(AV_PIX_FMT_YUV420P);
        }
    } else {
        // Handle planar YUV formats
        int uvWidth = AV_CEIL_RSHIFT(width, pixDesc->log2_chroma_w);
        int uvHeight = AV_CEIL_RSHIFT(height, pixDesc->log2_chroma_h);

        uint8_t* srcData[4] = {nullptr};
        int srcLinesize[4] = {};
        av_image_fill_arrays(srcData, srcLinesize, packetData, srcFmt, width, height, 1);
        uint8_t* dstData[4] = {frameData->yPtr(), frameData->uPtr(), frameData->vPtr(), nullptr};

        memcpy(dstData[0], srcData[0], width * height);
        memcpy(dstData[1], srcData[1], uvWidth * uvHeight);
        memcpy(dstData[2], srcData[2], uvWidth * uvHeight);
    }

    frameData->setPts(currentFrameIndex);

    if (!m_isY4M && codecContext && isYUV(codecContext->codec_id)) {
        if (currentFrameIndex == yuvTotalFrames - 1) {
            debug("vd", QString("%1 is end frame").arg(currentFrameIndex));
            frameData->setEndFrame(true);
            m_hitEndFrame = true;
        } else if (frameData->isEndFrame()) {
            debug("vd", QString("%1 is not end frame").arg(currentFrameIndex));
            frameData->setEndFrame(false);
        }
    }

    currentFrameIndex++;
}

int VideoDecoder::getTotalFrames() {
    if (m_isY4M && yuvTotalFrames > 0) {
        return yuvTotalFrames;
    }

    if (!m_isY4M && codecContext && isYUV(codecContext->codec_id) && yuvTotalFrames > 0) {
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

    // Stream Context
    AVStream* videoStream = formatContext->streams[videoStreamIndex];
    if (videoStream->duration != AV_NOPTS_VALUE) {
        return av_rescale_q(videoStream->duration, videoStream->time_base, AVRational{1, 1000});
    }

    // Format Context Fallback
    if (formatContext->duration > 0) {
        return av_rescale_q(formatContext->duration, AV_TIME_BASE_Q, AVRational{1, 1000});
    }

    // Estimation from framerate Fallback
    if (videoStream->nb_frames > 0 && videoStream->avg_frame_rate.num > 0) {
        double fps = av_q2d(videoStream->avg_frame_rate);
        return static_cast<int64_t>((videoStream->nb_frames / fps) * 1000.0);
    }

    return -1;
}

void VideoDecoder::seekTo(int64_t targetPts) {
    if (targetPts < 0) {
        warning("vd", QString("internal seek asked for negative pts: %1").arg(targetPts));
        targetPts = 0;
    }

    if (m_isY4M) {
        seekToY4M(targetPts);
    } else if (!formatContext || !codecContext || videoStreamIndex < 0) {
        ErrorReporter::instance().report("VideoDecoder not properly initialized for seeking", LogLevel::Error);
        return;
    } else if (isYUV(codecContext->codec_id)) {
        seekToYUV(targetPts);
    } else {
        seekToCompressed(targetPts);
    }
}

void VideoDecoder::seekToYUV(int64_t targetPts) {
    QFileInfo info(QString::fromStdString(m_fileName));
    int64_t fileSize = info.size();

    // Calculate frame size
    const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(codecContext->pix_fmt);
    int uvWidth = AV_CEIL_RSHIFT(m_width, pixDesc->log2_chroma_w);
    int uvHeight = AV_CEIL_RSHIFT(m_height, pixDesc->log2_chroma_h);
    int ySize = m_width * m_height;
    int uvSize = uvWidth * uvHeight;
    int frameSize = ySize + 2 * uvSize;

    // Recalculate total frames based on actual file size
    int64_t actualTotalFrames = fileSize / frameSize;
    if (actualTotalFrames != yuvTotalFrames) {
        warning("vd",
                QString("YUV file size mismatch - calculated: %1 actual: %2 fileSize: %3 frameSize: %4")
                    .arg(yuvTotalFrames)
                    .arg(actualTotalFrames)
                    .arg(fileSize)
                    .arg(frameSize));
        yuvTotalFrames = actualTotalFrames;
    }

    // Calculate byte position for target frame
    int64_t bytePosition = targetPts * frameSize;

    // First, try to seek to the beginning to ensure we have a clean state
    int ret = avio_seek(formatContext->pb, 0, SEEK_SET);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report("Failed to seek to beginning of YUV file (error: " + std::string(errBuf) + ")",
                                         LogLevel::Error);
        return;
    }

    // Now seek to the calculated position
    ret = avio_seek(formatContext->pb, bytePosition, SEEK_SET);
    if (ret < 0) {
        debug("vd", "Direct seek failed, trying alternative method");

        // Try seeking from current position
        int64_t currentPos = avio_tell(formatContext->pb);
        if (currentPos >= 0) {
            int64_t offset = bytePosition - currentPos;
            if (offset > 0) {
                ret = avio_seek(formatContext->pb, offset, SEEK_CUR);
            } else if (offset < 0) {
                ret = avio_seek(formatContext->pb, -offset, SEEK_CUR);
            } else {
                ret = 0; // Already at correct position
            }
        }

        if (ret < 0) {
            char errBuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errBuf, AV_ERROR_MAX_STRING_SIZE);
            ErrorReporter::instance().report("Failed to seek in YUV file to frame: " + std::to_string(targetPts) +
                                                 " (error: " + std::string(errBuf) + ")",
                                             LogLevel::Error);
            return;
        }
    }

    currentFrameIndex = targetPts;
    debug("vd", QString("Successfully seeked to frame %1").arg(targetPts));
    return;
}

void VideoDecoder::seekToCompressed(int64_t targetPts) {
    int64_t seek_timestamp = targetPts;
    if (m_needsTimebaseConversion) {
        AVStream* videoStream = formatContext->streams[videoStreamIndex];
        double timestamp_seconds = targetPts / m_framerate;
        seek_timestamp = llrint(timestamp_seconds / av_q2d(videoStream->time_base));

        debug("vd",
              QString("Decoder::seekTo frame %1 -> time %2s -> stream_ts %3")
                  .arg(targetPts)
                  .arg(timestamp_seconds)
                  .arg(seek_timestamp));
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
            debug("vd", QString("seekTo reached EOF while seeking to frame %1").arg(targetPts));
            break;
        }

        if (packet->stream_index == videoStreamIndex) {
            ret = avcodec_send_packet(codecContext, packet);
            if (ret < 0) {
                warning("vd", "seekTo failed to send packet to decoder");
                av_packet_free(&packet);
                av_frame_free(&frame);
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
                debug("vd",
                      QString("Decoder::seekTo decoded frame with PTS: %1 target: %2").arg(current_pts).arg(targetPts));
            }
        }
        av_packet_unref(packet);
        av_packet_free(&packet);
        av_frame_free(&frame);
    }

    currentFrameIndex = targetPts;
}

void VideoDecoder::seek(int64_t targetPts, int loadCount) {
    debug("vd", QString("seek called with targetPts: %1").arg(targetPts));

    // For Y4M and YUV files, check against total frames
    if ((m_isY4M || (codecContext && isYUV(codecContext->codec_id))) && yuvTotalFrames > 0) {
        if (targetPts >= yuvTotalFrames) {
            warning("vd",
                    QString("seek - Target PTS %1 exceeds total frames %2, adjusting to last frame")
                        .arg(targetPts)
                        .arg(yuvTotalFrames));
            targetPts = yuvTotalFrames - 1;
        }
    }

    if (loadCount != -1) {
        // Load frames for stepping
        seekTo(targetPts);
        loadFrames(loadCount);
    } else {
        // Load past & future frames around the target PTS for seeking
        int64_t startPts = std::max(targetPts - m_frameQueue->getSize() / 4, int64_t{0});
        seekTo(startPts);
        debug("vd", QString("Seeking to currentFrameIndex: %1").arg(currentFrameIndex));
        loadFrames(m_frameQueue->getSize() / 2);
        debug("vd", QString("Loaded until currentFrameIndex: %1").arg(currentFrameIndex));
    }

    emit frameSeeked(targetPts);
}

int64_t VideoDecoder::loadY4MFrame() {
    if (!m_isY4M || !m_y4mInfo.isValid) {
        ErrorReporter::instance().report("Y4M format not properly initialized", LogLevel::Error);
        return -1;
    }

    QFile file(QString::fromStdString(m_fileName));
    if (!file.open(QIODevice::ReadOnly)) {
        ErrorReporter::instance().report("Cannot open Y4M file for reading", LogLevel::Error);
        return -1;
    }

    // Calculate current frame position in file
    int frameDataSize = Y4MParser::calculateFrameSize(m_y4mInfo);
    int totalFrameSize = 6 + frameDataSize; // "FRAME\n" + frame data
    int64_t framePosition = m_y4mInfo.headerSize + currentFrameIndex * totalFrameSize;

    // Check if beyond file end
    if (framePosition >= file.size()) {
        file.close();
        return -1; // EOF
    }

    // Seek to frame position
    if (!file.seek(framePosition)) {
        ErrorReporter::instance().report("Cannot seek to frame position in Y4M file", LogLevel::Error);
        file.close();
        return -1;
    }

    // Read "FRAME\n" prefix
    QByteArray frameHeader = file.read(6);
    if (frameHeader != "FRAME\n") {
        ErrorReporter::instance().report("Invalid Y4M frame header", LogLevel::Error);
        file.close();
        return -1;
    }

    // Read frame data
    QByteArray frameData = file.read(frameDataSize);
    if (frameData.size() != frameDataSize) {
        ErrorReporter::instance().report("Incomplete Y4M frame data", LogLevel::Error);
        file.close();
        return -1;
    }

    file.close();

    // Copy frame data to FrameData structure
    int64_t pts = currentFrameIndex;
    FrameData* outputFrame = m_frameQueue->getTailFrame(pts);
    if (!outputFrame) {
        ErrorReporter::instance().report("Cannot get frame from queue", LogLevel::Error);
        return -1;
    }

    copyY4MFrame(frameData, outputFrame);

    outputFrame->setPts(pts);

    // Check if this is the last frame
    if (currentFrameIndex == yuvTotalFrames - 1) {
        outputFrame->setEndFrame(true);
        debug("vd", QString("Y4M frame %1 is end frame").arg(currentFrameIndex));
        m_hitEndFrame = true;
    } else {
        outputFrame->setEndFrame(false);
    }

    currentFrameIndex++;

    debug("vd", QString("Y4M loaded frame %1").arg(pts));
    return pts;
}

void VideoDecoder::copyY4MFrame(const QByteArray& frameData, FrameData* outputFrame) {
    if (!outputFrame || frameData.isEmpty()) {
        return;
    }

    const uint8_t* srcData = reinterpret_cast<const uint8_t*>(frameData.constData());
    uint8_t* yPtr = outputFrame->yPtr();
    uint8_t* uPtr = outputFrame->uPtr();
    uint8_t* vPtr = outputFrame->vPtr();

    if (!yPtr || !uPtr || !vPtr) {
        ErrorReporter::instance().report("Invalid frame data pointers", LogLevel::Error);
        return;
    }

    int width = m_y4mInfo.width;
    int height = m_y4mInfo.height;

    // Copy data based on pixel format
    const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(m_y4mInfo.pixelFormat);
    if (!pixDesc) {
        ErrorReporter::instance().report("Invalid pixel format descriptor", LogLevel::Error);
        return;
    }

    int uvWidth = AV_CEIL_RSHIFT(width, pixDesc->log2_chroma_w);
    int uvHeight = AV_CEIL_RSHIFT(height, pixDesc->log2_chroma_h);

    // Y plane
    int ySize = width * height;
    memcpy(yPtr, srcData, ySize);

    // U and V planes
    int uvSize = uvWidth * uvHeight;
    memcpy(uPtr, srcData + ySize, uvSize);
    memcpy(vPtr, srcData + ySize + uvSize, uvSize);

    debug("vd", QString("Y4M frame copied - Y size: %1, UV size: %2").arg(ySize).arg(uvSize));
}

void VideoDecoder::seekToY4M(int64_t targetPts) {
    if (!m_isY4M || !m_y4mInfo.isValid) {
        ErrorReporter::instance().report("Y4M format not properly initialized for seeking", LogLevel::Error);
        return;
    }

    if (targetPts < 0) {
        targetPts = 0;
    }

    if (targetPts >= yuvTotalFrames) {
        targetPts = yuvTotalFrames - 1;
    }

    currentFrameIndex = targetPts;
    debug("vd", QString("Y4M seeked to frame %1").arg(targetPts));
}
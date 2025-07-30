#include "compareHelper.h"
#include <QDebug>
#include <QString>
#include <cmath>

extern "C" {
#include <libavutil/log.h>
}

CompareHelper::CompareHelper() :
    m_vmafGraph(nullptr),
    m_vmafBufferSrcCtx1(nullptr),
    m_vmafBufferSrcCtx2(nullptr),
    m_vmafBufferSinkCtx(nullptr),
    m_vmafFilterCtx(nullptr),
    m_ssimGraph(nullptr),
    m_ssimBufferSrcCtx1(nullptr),
    m_ssimBufferSrcCtx2(nullptr),
    m_ssimBufferSinkCtx(nullptr),
    m_ssimFilterCtx(nullptr),
    m_psnrGraph(nullptr),
    m_psnrBufferSrcCtx1(nullptr),
    m_psnrBufferSrcCtx2(nullptr),
    m_psnrBufferSinkCtx(nullptr),
    m_psnrFilterCtx(nullptr),
    m_initialized(false) {
    
    // Initialize filter graphs for different metrics
    bool vmafInit = initializeFilterGraph("libvmaf", &m_vmafGraph, &m_vmafBufferSrcCtx1, 
                                          &m_vmafBufferSrcCtx2, &m_vmafBufferSinkCtx, &m_vmafFilterCtx);
    bool ssimInit = initializeFilterGraph("ssim", &m_ssimGraph, &m_ssimBufferSrcCtx1, 
                                          &m_ssimBufferSrcCtx2, &m_ssimBufferSinkCtx, &m_ssimFilterCtx);
    bool psnrInit = initializeFilterGraph("psnr", &m_psnrGraph, &m_psnrBufferSrcCtx1, 
                                          &m_psnrBufferSrcCtx2, &m_psnrBufferSinkCtx, &m_psnrFilterCtx);
    
    m_initialized = vmafInit && ssimInit && psnrInit;
    
    if (!m_initialized) {
        ErrorReporter::instance().report("Failed to initialize one or more filter graphs", LogLevel::Warning);
    }
}

CompareHelper::~CompareHelper() {
    if (m_vmafGraph) {
        avfilter_graph_free(&m_vmafGraph);
    }
    if (m_ssimGraph) {
        avfilter_graph_free(&m_ssimGraph);
    }
    if (m_psnrGraph) {
        avfilter_graph_free(&m_psnrGraph);
    }
}

PSNRResult CompareHelper::getPSNR(FrameData* frame1, FrameData* frame2, FrameMeta* metadata1, FrameMeta* metadata2) {
    // Set FFmpeg log level to only show errors
    av_log_set_level(AV_LOG_ERROR);
    if (!frame1 || !frame2) {
        ErrorReporter::instance().report("Invalid frame data for PSNR calculation", LogLevel::Error);
        return PSNRResult();
    }

    AVFrame* avFrame1 = frameDataToAVFrame(frame1, metadata1);
    AVFrame* avFrame2 = frameDataToAVFrame(frame2, metadata2);

    if (!avFrame1 || !avFrame2) {
        if (avFrame1)
            av_frame_free(&avFrame1);
        if (avFrame2)
            av_frame_free(&avFrame2);
        return PSNRResult();
    }

    // Create filter graph
    AVFilterGraph* filterGraph = avfilter_graph_alloc();
    if (!filterGraph) {
        ErrorReporter::instance().report("Could not allocate filter graph", LogLevel::Error);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    // Get filter references
    const AVFilter* bufferSrc = avfilter_get_by_name("buffer");
    const AVFilter* bufferSink = avfilter_get_by_name("buffersink");
    const AVFilter* psnrFilter = avfilter_get_by_name("psnr");

    if (!bufferSrc || !bufferSink || !psnrFilter) {
        ErrorReporter::instance().report("Could not find required filters", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    AVFilterContext* bufferSrcCtx1 = nullptr;
    AVFilterContext* bufferSrcCtx2 = nullptr;
    AVFilterContext* bufferSinkCtx = nullptr;
    AVFilterContext* psnrFilterCtx = nullptr;

    // Create buffer source arguments with proper format
    char args1[512];
    char args2[512];
    snprintf(args1,
             sizeof(args1),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             avFrame1->width,
             avFrame1->height,
             avFrame1->format,
             1,
             25,
             1,
             1);
    snprintf(args2,
             sizeof(args2),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             avFrame2->width,
             avFrame2->height,
             avFrame2->format,
             1,
             25,
             1,
             1);

    // Create source contexts with proper arguments
    int ret = avfilter_graph_create_filter(&bufferSrcCtx1, bufferSrc, "src1", args1, nullptr, filterGraph);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not create buffer source 1: %1").arg(err_buf).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    ret = avfilter_graph_create_filter(&bufferSrcCtx2, bufferSrc, "src2", args2, nullptr, filterGraph);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not create buffer source 2: %1").arg(err_buf).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    // Create sink context
    ret = avfilter_graph_create_filter(&bufferSinkCtx, bufferSink, "sink", nullptr, nullptr, filterGraph);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not create buffer sink", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    // Create PSNR filter context
    ret = avfilter_graph_create_filter(&psnrFilterCtx, psnrFilter, "psnr", nullptr, nullptr, filterGraph);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(QString("Could not create PSNR filter: %1").arg(err_buf).toStdString().c_str(),
                                         LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    // Link filters
    ret = avfilter_link(bufferSrcCtx1, 0, psnrFilterCtx, 0);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not link source 1 to PSNR filter", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    ret = avfilter_link(bufferSrcCtx2, 0, psnrFilterCtx, 1);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not link source 2 to PSNR filter", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    ret = avfilter_link(psnrFilterCtx, 0, bufferSinkCtx, 0);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not link PSNR filter to sink", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    // Configure the graph
    ret = avfilter_graph_config(filterGraph, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not configure filter graph: %1").arg(err_buf).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    // Push frames to the filter graph
    ret = av_buffersrc_add_frame(bufferSrcCtx1, avFrame1);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not add frame 1 to buffer source: %1").arg(err_buf).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    ret = av_buffersrc_add_frame(bufferSrcCtx2, avFrame2);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not add frame 2 to buffer source: %1").arg(err_buf).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return PSNRResult();
    }

    // Signal end of input
    (void)av_buffersrc_add_frame(bufferSrcCtx1, nullptr);
    (void)av_buffersrc_add_frame(bufferSrcCtx2, nullptr);

    // Get the result
    AVFrame* resultFrame = av_frame_alloc();
    PSNRResult psnrResult;

    ret = av_buffersink_get_frame(bufferSinkCtx, resultFrame);
    if (ret >= 0) {
        // Extract PSNR values from frame metadata
        AVDictionaryEntry* entry = nullptr;

        // Get average PSNR - try multiple possible keys
        entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.psnr.avg", nullptr, 0);
        if (entry && entry->value) {
            psnrResult.average = atof(entry->value);
        } else {
            entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.psnr_avg", nullptr, 0);
            if (entry && entry->value) {
                psnrResult.average = atof(entry->value);
            } else {
                // Try MSE-based calculation for average
                entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.mse.avg", nullptr, 0);
                if (entry && entry->value) {
                    double mse = atof(entry->value);
                    if (mse > 0) {
                        psnrResult.average = 20 * log10(255.0 / sqrt(mse));
                    } else if (mse == 0) {
                        psnrResult.average = INFINITY; // Perfect match
                    }
                } else {
                    // Try calculating average from Y, U, V components if available
                    if (psnrResult.y >= 0 && psnrResult.u >= 0 && psnrResult.v >= 0) {
                        psnrResult.average = (psnrResult.y + psnrResult.u + psnrResult.v) / 3.0;
                    }
                }
            }
        }

        // Get Y component PSNR
        entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.psnr.y", nullptr, 0);
        if (entry && entry->value) {
            psnrResult.y = atof(entry->value);
        } else {
            entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.mse.y", nullptr, 0);
            if (entry && entry->value) {
                double mse = atof(entry->value);
                if (mse > 0) {
                    psnrResult.y = 20 * log10(255.0 / sqrt(mse));
                } else if (mse == 0) {
                    psnrResult.y = INFINITY;
                }
            }
        }

        // Get U component PSNR
        entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.psnr.u", nullptr, 0);
        if (entry && entry->value) {
            psnrResult.u = atof(entry->value);
        } else {
            entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.mse.u", nullptr, 0);
            if (entry && entry->value) {
                double mse = atof(entry->value);
                if (mse > 0) {
                    psnrResult.u = 20 * log10(255.0 / sqrt(mse));
                } else if (mse == 0) {
                    psnrResult.u = INFINITY;
                }
            }
        }

        // Get V component PSNR
        entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.psnr.v", nullptr, 0);
        if (entry && entry->value) {
            psnrResult.v = atof(entry->value);
        } else {
            entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.mse.v", nullptr, 0);
            if (entry && entry->value) {
                double mse = atof(entry->value);
                if (mse > 0) {
                    psnrResult.v = 20 * log10(255.0 / sqrt(mse));
                } else if (mse == 0) {
                    psnrResult.v = INFINITY;
                }
            }
        }
    } else {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not get frame from sink: %1").arg(err_buf).toStdString().c_str(), LogLevel::Error);
    }

    // Cleanup
    av_frame_free(&resultFrame);
    avfilter_graph_free(&filterGraph);
    av_frame_free(&avFrame1);
    av_frame_free(&avFrame2);

    return psnrResult;
}

double CompareHelper::getVMAF(FrameData* frame1, FrameData* frame2, FrameMeta* metadata1, FrameMeta* metadata2) {
    return calculateMetricWithFilter(frame1, frame2, metadata1, metadata2, "libvmaf");
}

double CompareHelper::getSSIM(FrameData* frame1, FrameData* frame2, FrameMeta* metadata1, FrameMeta* metadata2) {
    return calculateMetricWithFilter(frame1, frame2, metadata1, metadata2, "ssim");
}

AVFrame* CompareHelper::frameDataToAVFrame(FrameData* frameData, FrameMeta* metadata) {
    if (!frameData) {
        return nullptr;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        ErrorReporter::instance().report("Could not allocate AVFrame", LogLevel::Error);
        return nullptr;
    }

    frame->format = metadata->format();
    frame->width = metadata->yWidth();
    frame->height = metadata->yHeight();
    frame->pts = frameData->pts();

    // Calculate plane sizes
    const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get((AVPixelFormat)frame->format);
    if (!pixDesc) {
        ErrorReporter::instance().report("Could not get pixel format descriptor", LogLevel::Error);
        av_frame_free(&frame);
        return nullptr;
    }

    int uvWidth = AV_CEIL_RSHIFT(frame->width, pixDesc->log2_chroma_w);
    int uvHeight = AV_CEIL_RSHIFT(frame->height, pixDesc->log2_chroma_h);

    // Allocate frame buffer
    if (av_frame_get_buffer(frame, 32) < 0) {
        ErrorReporter::instance().report("Could not allocate frame buffer", LogLevel::Error);
        av_frame_free(&frame);
        return nullptr;
    }

    // Make frame writable
    if (av_frame_make_writable(frame) < 0) {
        ErrorReporter::instance().report("Could not make frame writable", LogLevel::Error);
        av_frame_free(&frame);
        return nullptr;
    }

    // Copy data from FrameData to AVFrame with proper strides
    // Y plane
    for (int y = 0; y < frame->height; y++) {
        memcpy(frame->data[0] + y * frame->linesize[0], frameData->yPtr() + y * frame->width, frame->width);
    }

    // U plane
    for (int y = 0; y < uvHeight; y++) {
        memcpy(frame->data[1] + y * frame->linesize[1], frameData->uPtr() + y * uvWidth, uvWidth);
    }

    // V plane
    for (int y = 0; y < uvHeight; y++) {
        memcpy(frame->data[2] + y * frame->linesize[2], frameData->vPtr() + y * uvWidth, uvWidth);
    }

    return frame;
}

double CompareHelper::calculateMetricWithFilter(
    FrameData* frame1, FrameData* frame2, FrameMeta* metadata1, FrameMeta* metadata2, const char* filterName) {
    
    if (!m_initialized) {
        ErrorReporter::instance().report("Filter graphs not initialized", LogLevel::Error);
        return -1.0;
    }
    
    // Set FFmpeg log level to only show errors
    av_log_set_level(AV_LOG_ERROR);
    if (!frame1 || !frame2) {
        ErrorReporter::instance().report("Invalid frame data for metric calculation", LogLevel::Error);
        return -1.0;
    }

    AVFrame* avFrame1 = frameDataToAVFrame(frame1, metadata1);
    AVFrame* avFrame2 = frameDataToAVFrame(frame2, metadata2);

    if (!avFrame1 || !avFrame2) {
        if (avFrame1)
            av_frame_free(&avFrame1);
        if (avFrame2)
            av_frame_free(&avFrame2);
        return -1.0;
    }

    // Select the appropriate pre-initialized filter graph
    AVFilterContext* bufferSrcCtx1 = nullptr;
    AVFilterContext* bufferSrcCtx2 = nullptr;
    AVFilterContext* bufferSinkCtx = nullptr;
    
    if (strcmp(filterName, "libvmaf") == 0) {
        bufferSrcCtx1 = m_vmafBufferSrcCtx1;
        bufferSrcCtx2 = m_vmafBufferSrcCtx2;
        bufferSinkCtx = m_vmafBufferSinkCtx;
    } else if (strcmp(filterName, "ssim") == 0) {
        bufferSrcCtx1 = m_ssimBufferSrcCtx1;
        bufferSrcCtx2 = m_ssimBufferSrcCtx2;
        bufferSinkCtx = m_ssimBufferSinkCtx;
    } else if (strcmp(filterName, "psnr") == 0) {
        bufferSrcCtx1 = m_psnrBufferSrcCtx1;
        bufferSrcCtx2 = m_psnrBufferSrcCtx2;
        bufferSinkCtx = m_psnrBufferSinkCtx;
    } else {
        ErrorReporter::instance().report(QString("Unsupported filter: %1").arg(filterName).toStdString().c_str(), LogLevel::Error);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    // Update buffer source parameters if frame dimensions changed
    char args1[512];
    char args2[512];
    snprintf(args1,
             sizeof(args1),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             avFrame1->width,
             avFrame1->height,
             avFrame1->format,
             1,
             25,
             1,
             1);
    snprintf(args2,
             sizeof(args2),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             avFrame2->width,
             avFrame2->height,
             avFrame2->format,
             1,
             25,
             1,
             1);

    // Note: In a more sophisticated implementation, you might want to cache these configurations
    // or recreate the filter graph only when parameters change
    
    // Push frames to the filter graph
    int ret = av_buffersrc_add_frame(bufferSrcCtx1, avFrame1);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not add frame 1 to buffer source: %1").arg(err_buf).toStdString().c_str(), LogLevel::Error);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    ret = av_buffersrc_add_frame(bufferSrcCtx2, avFrame2);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not add frame 2 to buffer source: %1").arg(err_buf).toStdString().c_str(), LogLevel::Error);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    // Signal end of input
    (void)av_buffersrc_add_frame(bufferSrcCtx1, nullptr);
    (void)av_buffersrc_add_frame(bufferSrcCtx2, nullptr);

    // Get the result
    AVFrame* resultFrame = av_frame_alloc();
    double metricValue = -1.0;

    ret = av_buffersink_get_frame(bufferSinkCtx, resultFrame);
    if (ret >= 0) {
        // Extract metric value from frame metadata
        AVDictionaryEntry* entry = nullptr;

        if (strcmp(filterName, "psnr") == 0) {
            // Try different PSNR metadata keys
            entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.mse.avg", nullptr, 0);
            if (entry && entry->value) {
                double mse = atof(entry->value);
                if (mse > 0) {
                    metricValue = 20 * log10(255.0 / sqrt(mse));
                } else if (mse == 0) {
                    metricValue = INFINITY; // Perfect match
                }
            } else {
                // Try other PSNR keys
                entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.psnr.avg", nullptr, 0);
                if (entry && entry->value) {
                    metricValue = atof(entry->value);
                }
            }
        } else if (strcmp(filterName, "ssim") == 0) {
            entry = av_dict_get(resultFrame->metadata, "lavfi.ssim.All", nullptr, 0);
            if (entry && entry->value) {
                metricValue = atof(entry->value);
            }
        } else {
            entry = av_dict_get(resultFrame->metadata, filterName, nullptr, 0);
            if (entry && entry->value) {
                metricValue = atof(entry->value);
            }
        }
    } else {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not get frame from sink: %1").arg(err_buf).toStdString().c_str(), LogLevel::Error);
    }

    // Cleanup only the frames (filter graph is persistent)
    av_frame_free(&resultFrame);
    av_frame_free(&avFrame1);
    av_frame_free(&avFrame2);

    return metricValue;
}

bool CompareHelper::initializeFilterGraph(const char* filterName, AVFilterGraph** graph, 
                                         AVFilterContext** bufferSrcCtx1, AVFilterContext** bufferSrcCtx2,
                                         AVFilterContext** bufferSinkCtx, AVFilterContext** metricFilterCtx) {
    // Create filter graph
    *graph = avfilter_graph_alloc();
    if (!*graph) {
        ErrorReporter::instance().report(QString("Could not allocate filter graph for %1").arg(filterName).toStdString().c_str(), LogLevel::Error);
        return false;
    }

    // Get filter references
    const AVFilter* bufferSrc = avfilter_get_by_name("buffer");
    const AVFilter* bufferSink = avfilter_get_by_name("buffersink");
    const AVFilter* metricFilter = avfilter_get_by_name(filterName);

    if (!bufferSrc || !bufferSink || !metricFilter) {
        ErrorReporter::instance().report(QString("Could not find required filters for %1").arg(filterName).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(graph);
        return false;
    }

    // Create buffer source contexts with dummy parameters (will be updated when processing frames)
    char args[512];
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             352, 288, AV_PIX_FMT_YUV420P, 1, 25, 1, 1);

    int ret = avfilter_graph_create_filter(bufferSrcCtx1, bufferSrc, "src1", args, nullptr, *graph);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not create buffer source 1 for %1: %2").arg(filterName).arg(err_buf).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(graph);
        return false;
    }

    ret = avfilter_graph_create_filter(bufferSrcCtx2, bufferSrc, "src2", args, nullptr, *graph);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not create buffer source 2 for %1: %2").arg(filterName).arg(err_buf).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(graph);
        return false;
    }

    // Create sink context
    ret = avfilter_graph_create_filter(bufferSinkCtx, bufferSink, "sink", nullptr, nullptr, *graph);
    if (ret < 0) {
        ErrorReporter::instance().report(QString("Could not create buffer sink for %1").arg(filterName).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(graph);
        return false;
    }

    // Create metric filter context
    const char* filterArgs = nullptr;
    if (strcmp(filterName, "libvmaf") == 0) {
        filterArgs = "log_path=/tmp/vmaf.log:log_fmt=json";
    }

    ret = avfilter_graph_create_filter(metricFilterCtx, metricFilter, "metric", filterArgs, nullptr, *graph);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not create metric filter %1: %2").arg(filterName).arg(err_buf).toStdString().c_str(),
            LogLevel::Error);
        avfilter_graph_free(graph);
        return false;
    }

    // Link filters
    ret = avfilter_link(*bufferSrcCtx1, 0, *metricFilterCtx, 0);
    if (ret < 0) {
        ErrorReporter::instance().report(QString("Could not link source 1 to metric filter for %1").arg(filterName).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(graph);
        return false;
    }

    ret = avfilter_link(*bufferSrcCtx2, 0, *metricFilterCtx, 1);
    if (ret < 0) {
        ErrorReporter::instance().report(QString("Could not link source 2 to metric filter for %1").arg(filterName).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(graph);
        return false;
    }

    ret = avfilter_link(*metricFilterCtx, 0, *bufferSinkCtx, 0);
    if (ret < 0) {
        ErrorReporter::instance().report(QString("Could not link metric filter to sink for %1").arg(filterName).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(graph);
        return false;
    }

    // Configure the graph
    ret = avfilter_graph_config(*graph, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
        ErrorReporter::instance().report(
            QString("Could not configure filter graph for %1: %2").arg(filterName).arg(err_buf).toStdString().c_str(), LogLevel::Error);
        avfilter_graph_free(graph);
        return false;
    }

    return true;
}

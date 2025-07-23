#include "compareHelper.h"


double CompareHelper::getPSNR(FrameData *frame1, FrameData *frame2, FrameMeta *metadata1, FrameMeta *metadata2) {
    return calculateMetricWithFilter(frame1, frame2, metadata1, metadata2, "psnr");
}

double CompareHelper::getVMAF(FrameData *frame1, FrameData *frame2, FrameMeta *metadata1, FrameMeta *metadata2) {
    return calculateMetricWithFilter(frame1, frame2, metadata1, metadata2, "libvmaf");
}

double CompareHelper::getSSIM(FrameData *frame1, FrameData *frame2, FrameMeta *metadata1, FrameMeta *metadata2) {
    return calculateMetricWithFilter(frame1, frame2, metadata1, metadata2, "ssim");
}

AVFrame *CompareHelper::frameDataToAVFrame(FrameData *frameData, FrameMeta *metadata) {
    if (!frameData) {
        return nullptr;
    }

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        ErrorReporter::instance().report("Could not allocate AVFrame", LogLevel::Error);
        return nullptr;
    }

    frame->format = metadata->format();
    frame->width = metadata->yWidth();
    frame->height = metadata->yHeight();
    frame->pts = frameData->pts();

    // Calculate plane sizes
    const AVPixFmtDescriptor *pixDesc = av_pix_fmt_desc_get((AVPixelFormat) frame->format);
    int uvWidth = AV_CEIL_RSHIFT(frame->width, pixDesc->log2_chroma_w);
    int uvHeight = AV_CEIL_RSHIFT(frame->height, pixDesc->log2_chroma_h);

    // Allocate frame buffer
    if (av_frame_get_buffer(frame, 32) < 0) {
        ErrorReporter::instance().report("Could not allocate frame buffer", LogLevel::Error);
        av_frame_free(&frame);
        return nullptr;
    }

    // Copy data from FrameData to AVFrame
    memcpy(frame->data[0], frameData->yPtr(), frame->width * frame->height);
    memcpy(frame->data[1], frameData->uPtr(), uvWidth * uvHeight);
    memcpy(frame->data[2], frameData->vPtr(), uvWidth * uvHeight);

    return frame;
}

double CompareHelper::calculateMetricWithFilter(FrameData *frame1, FrameData *frame2, FrameMeta *metadata1,
                                                FrameMeta *metadata2, const char *filterName) {
    if (!frame1 || !frame2) {
        ErrorReporter::instance().report("Invalid frame data for metric calculation", LogLevel::Error);
        return -1.0;
    }

    AVFrame *avFrame1 = frameDataToAVFrame(frame1, metadata1);
    AVFrame *avFrame2 = frameDataToAVFrame(frame2, metadata2);

    if (!avFrame1 || !avFrame2) {
        if (avFrame1) av_frame_free(&avFrame1);
        if (avFrame2) av_frame_free(&avFrame2);
        return -1.0;
    }

    // Create filter graph
    AVFilterGraph *filterGraph = avfilter_graph_alloc();
    if (!filterGraph) {
        ErrorReporter::instance().report("Could not allocate filter graph", LogLevel::Error);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    // Get filter references
    const AVFilter *bufferSrc = avfilter_get_by_name("buffer");
    const AVFilter *bufferSink = avfilter_get_by_name("buffersink");
    const AVFilter *metricFilter = avfilter_get_by_name(filterName);

    if (!bufferSrc || !bufferSink || !metricFilter) {
        ErrorReporter::instance().report("Could not find required filters", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    AVFilterContext *bufferSrcCtx1 = nullptr;
    AVFilterContext *bufferSrcCtx2 = nullptr;
    AVFilterContext *bufferSinkCtx = nullptr;
    AVFilterContext *metricFilterCtx = nullptr;

    // Create source contexts
    int ret = avfilter_graph_create_filter(&bufferSrcCtx1, bufferSrc, "src1", nullptr, nullptr, filterGraph);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not create buffer source 1", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    ret = avfilter_graph_create_filter(&bufferSrcCtx2, bufferSrc, "src2", nullptr, nullptr, filterGraph);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not create buffer source 2", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    // Create sink context
    ret = avfilter_graph_create_filter(&bufferSinkCtx, bufferSink, "sink", nullptr, nullptr, filterGraph);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not create buffer sink", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    // Create metric filter context
    const char *filterArgs = nullptr;
    if (strcmp(filterName, "libvmaf") == 0) {
        filterArgs = "log_path=/tmp/vmaf.log:log_fmt=json";
    }

    ret = avfilter_graph_create_filter(&metricFilterCtx, metricFilter, "metric", filterArgs, nullptr, filterGraph);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not create metric filter", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    // Link filters
    ret = avfilter_link(bufferSrcCtx1, 0, metricFilterCtx, 0);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not link source 1 to metric filter", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    ret = avfilter_link(bufferSrcCtx2, 0, metricFilterCtx, 1);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not link source 2 to metric filter", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    ret = avfilter_link(metricFilterCtx, 0, bufferSinkCtx, 0);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not link metric filter to sink", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    // Configure the graph
    ret = avfilter_graph_config(filterGraph, nullptr);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not configure filter graph", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    // Push frames to the filter graph
    ret = av_buffersrc_add_frame(bufferSrcCtx1, avFrame1);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not add frame 1 to buffer source", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    ret = av_buffersrc_add_frame(bufferSrcCtx2, avFrame2);
    if (ret < 0) {
        ErrorReporter::instance().report("Could not add frame 2 to buffer source", LogLevel::Error);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1.0;
    }

    // Signal end of input
    av_buffersrc_add_frame(bufferSrcCtx1, nullptr);
    av_buffersrc_add_frame(bufferSrcCtx2, nullptr);

    // Get the result
    AVFrame *resultFrame = av_frame_alloc();
    double metricValue = -1.0;

    ret = av_buffersink_get_frame(bufferSinkCtx, resultFrame);
    if (ret >= 0) {
        // Extract metric value from frame metadata
        AVDictionaryEntry *entry = av_dict_get(resultFrame->metadata, filterName, nullptr, 0);
        if (entry && entry->value) {
            metricValue = atof(entry->value);
        } else {
            // For some filters, the metric might be stored differently
            if (strcmp(filterName, "psnr") == 0) {
                entry = av_dict_get(resultFrame->metadata, "lavfi.psnr.mse.avg", nullptr, 0);
                if (entry && entry->value) {
                    double mse = atof(entry->value);
                    if (mse > 0) {
                        metricValue = 20 * log10(255.0 / sqrt(mse));
                    }
                }
            } else if (strcmp(filterName, "ssim") == 0) {
                entry = av_dict_get(resultFrame->metadata, "lavfi.ssim.All", nullptr, 0);
                if (entry && entry->value) {
                    metricValue = atof(entry->value);
                }
            }
        }
    }

    // Cleanup
    av_frame_free(&resultFrame);
    avfilter_graph_free(&filterGraph);
    av_frame_free(&avFrame1);
    av_frame_free(&avFrame2);

    return metricValue;
}

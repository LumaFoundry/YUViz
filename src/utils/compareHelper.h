#pragma once
#include "errorReporter.h"
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "psnrResult.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class CompareHelper {
  public:
    CompareHelper();
    ~CompareHelper();

    PSNRResult getPSNR(FrameData* frame1, FrameData* frame2, FrameMeta* metadata1, FrameMeta* metadata2);

    double getVMAF(FrameData* frame1, FrameData* frame2, FrameMeta* metadata1, FrameMeta* metadata2);

    double getSSIM(FrameData* frame1, FrameData* frame2, FrameMeta* metadata1, FrameMeta* metadata2);

  private:
    AVFrame* frameDataToAVFrame(FrameData* frameData, FrameMeta* metadata);

    double calculateMetricWithFilter(
        FrameData* frame1, FrameData* frame2, FrameMeta* metadata1, FrameMeta* metadata2, const char* filterName);

    bool initializeFilterGraph(const char* filterName,
                               AVFilterGraph** graph,
                               AVFilterContext** bufferSrcCtx1,
                               AVFilterContext** bufferSrcCtx2,
                               AVFilterContext** bufferSinkCtx,
                               AVFilterContext** metricFilterCtx);

    // Filter graphs for different metrics
    AVFilterGraph* m_vmafGraph;
    AVFilterContext* m_vmafBufferSrcCtx1;
    AVFilterContext* m_vmafBufferSrcCtx2;
    AVFilterContext* m_vmafBufferSinkCtx;
    AVFilterContext* m_vmafFilterCtx;

    AVFilterGraph* m_ssimGraph;
    AVFilterContext* m_ssimBufferSrcCtx1;
    AVFilterContext* m_ssimBufferSrcCtx2;
    AVFilterContext* m_ssimBufferSinkCtx;
    AVFilterContext* m_ssimFilterCtx;

    AVFilterGraph* m_psnrGraph;
    AVFilterContext* m_psnrBufferSrcCtx1;
    AVFilterContext* m_psnrBufferSrcCtx2;
    AVFilterContext* m_psnrBufferSinkCtx;
    AVFilterContext* m_psnrFilterCtx;

    bool m_initialized;
};

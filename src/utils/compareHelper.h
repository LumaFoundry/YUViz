#pragma once
#include "frameData.h"
#include "errorReporter.h"
#include "frameMeta.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

class CompareHelper {
public:
    double getPSNR(FrameData *frame1, FrameData *frame2, FrameMeta *metadata1, FrameMeta *metadata2);

    double getVMAF(FrameData *frame1, FrameData *frame2, FrameMeta *metadata1, FrameMeta *metadata2);

    double getSSIM(FrameData *frame1, FrameData *frame2, FrameMeta *metadata1, FrameMeta *metadata2);

private:
    AVFrame *frameDataToAVFrame(FrameData *frameData, FrameMeta *metadata);

    double calculateMetricWithFilter(FrameData *frame1, FrameData *frame2, FrameMeta *metadata1, FrameMeta *metadata2,
                                     const char *filterName);
};

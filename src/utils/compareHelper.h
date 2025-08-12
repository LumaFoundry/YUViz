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

};

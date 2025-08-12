#include "compareHelper.h"
#include <QDebug>
#include <QString>
#include <cmath>
#include <limits>

CompareHelper::CompareHelper() = default;
CompareHelper::~CompareHelper() = default;

extern "C" {
#include <libavutil/log.h>
}

// Optimized sum of squared differences (SSD) with simple unrolling & multiple accumulators.
static inline uint64_t sumSquaredDiff(const uint8_t* __restrict p1, const uint8_t* __restrict p2, size_t count) {
    if (!p1 || !p2 || count == 0)
        return UINT64_C(0);

    size_t i = 0;
    uint64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;

    // Process 32 bytes per iteration (4 * 8) to balance unrolling without code bloat
    for (; i + 32 <= count; i += 32) {
#define SSD_STEP(k)                                                                                                    \
    {                                                                                                                  \
        int d = int(p1[i + k]) - int(p2[i + k]);                                                                       \
        s0 += uint64_t(d * d);                                                                                         \
    }
        SSD_STEP(0)
        SSD_STEP(1)
        SSD_STEP(2) SSD_STEP(3) SSD_STEP(4) SSD_STEP(5) SSD_STEP(6) SSD_STEP(7)
#undef SSD_STEP
#define SSD_STEP(k)                                                                                                    \
    {                                                                                                                  \
        int d = int(p1[i + 8 + k]) - int(p2[i + 8 + k]);                                                               \
        s1 += uint64_t(d * d);                                                                                         \
    }
            SSD_STEP(0) SSD_STEP(1) SSD_STEP(2) SSD_STEP(3) SSD_STEP(4) SSD_STEP(5) SSD_STEP(6) SSD_STEP(7)
#undef SSD_STEP
#define SSD_STEP(k)                                                                                                    \
    {                                                                                                                  \
        int d = int(p1[i + 16 + k]) - int(p2[i + 16 + k]);                                                             \
        s2 += uint64_t(d * d);                                                                                         \
    }
                SSD_STEP(0) SSD_STEP(1) SSD_STEP(2) SSD_STEP(3) SSD_STEP(4) SSD_STEP(5) SSD_STEP(6) SSD_STEP(7)
#undef SSD_STEP
#define SSD_STEP(k)                                                                                                    \
    {                                                                                                                  \
        int d = int(p1[i + 24 + k]) - int(p2[i + 24 + k]);                                                             \
        s3 += uint64_t(d * d);                                                                                         \
    }
                    SSD_STEP(0) SSD_STEP(1) SSD_STEP(2) SSD_STEP(3) SSD_STEP(4) SSD_STEP(5) SSD_STEP(6) SSD_STEP(7)
#undef SSD_STEP
    }

    // Tail (still encourage vectorization for medium tails)
    for (; i + 8 <= count; i += 8) {
        int d0 = int(p1[i + 0]) - int(p2[i + 0]);
        s0 += uint64_t(d0 * d0);
        int d1 = int(p1[i + 1]) - int(p2[i + 1]);
        s1 += uint64_t(d1 * d1);
        int d2 = int(p1[i + 2]) - int(p2[i + 2]);
        s2 += uint64_t(d2 * d2);
        int d3 = int(p1[i + 3]) - int(p2[i + 3]);
        s3 += uint64_t(d3 * d3);
        int d4 = int(p1[i + 4]) - int(p2[i + 4]);
        s0 += uint64_t(d4 * d4);
        int d5 = int(p1[i + 5]) - int(p2[i + 5]);
        s1 += uint64_t(d5 * d5);
        int d6 = int(p1[i + 6]) - int(p2[i + 6]);
        s2 += uint64_t(d6 * d6);
        int d7 = int(p1[i + 7]) - int(p2[i + 7]);
        s3 += uint64_t(d7 * d7);
    }

    for (; i < count; ++i) {
        int d = int(p1[i]) - int(p2[i]);
        s0 += uint64_t(d * d);
    }

    return s0 + s1 + s2 + s3;
}

PSNRResult CompareHelper::getPSNR(FrameData* frame1, FrameData* frame2, FrameMeta* metadata1, FrameMeta* metadata2) {
    // // Basic validation
    // if (!frame1 || !frame2 || !metadata1 || !metadata2) {
    //     ErrorReporter::instance().report("CompareHelper::getPSNR - null argument", LogLevel::Warning);
    //     return {};
    // }
    //
    // // Ensure same dimensions & format (expecting YUV420P planar)
    // if (metadata1->yWidth() != metadata2->yWidth() || metadata1->yHeight() != metadata2->yHeight() ||
    //     metadata1->uvWidth() != metadata2->uvWidth() || metadata1->uvHeight() != metadata2->uvHeight() ||
    //     metadata1->format() != metadata2->format()) {
    //     ErrorReporter::instance().report("CompareHelper::getPSNR - incompatible frame metadata", LogLevel::Warning);
    //     return {};
    // }

    int yW = metadata1->yWidth();
    int yH = metadata1->yHeight();
    int uvW = metadata1->uvWidth();
    int uvH = metadata1->uvHeight();

    size_t yCount = static_cast<size_t>(yW) * static_cast<size_t>(yH);
    size_t uvCount = static_cast<size_t>(uvW) * static_cast<size_t>(uvH);

    const uint8_t* y1 = frame1->yPtr();
    const uint8_t* u1 = frame1->uPtr();
    const uint8_t* v1 = frame1->vPtr();
    const uint8_t* y2 = frame2->yPtr();
    const uint8_t* u2 = frame2->uPtr();
    const uint8_t* v2 = frame2->vPtr();

    if (!y1 || !u1 || !v1 || !y2 || !u2 || !v2) {
        ErrorReporter::instance().report("CompareHelper::getPSNR - null plane pointer", LogLevel::Warning);
        return {};
    }

    // Determine bit depth from pixel format (assume 8 if descriptor unavailable)
    int bitDepth = 8;

    double maxSampleValue = static_cast<double>((1ULL << bitDepth) - 1ULL);

    uint64_t ySSD = sumSquaredDiff(y1, y2, yCount);
    uint64_t uSSD = sumSquaredDiff(u1, u2, uvCount);
    uint64_t vSSD = sumSquaredDiff(v1, v2, uvCount);

    if (ySSD == 0 && uSSD == 0 && vSSD == 0) {
        return PSNRResult(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    }

    // If any plane pointer invalid we would have returned earlier; here SSD of 0 means identical plane.
    double yPSNR = 10.0 * std::log10((maxSampleValue * maxSampleValue) / (double(ySSD) / double(yCount)));
    double uPSNR = 10.0 * std::log10((maxSampleValue * maxSampleValue) / (double(uSSD) / double(uvCount)));
    double vPSNR = 10.0 * std::log10((maxSampleValue * maxSampleValue) / (double(vSSD) / double(uvCount)));

    // Weighted average PSNR across all samples: compute overall MSE then convert
    long double totalSamples = static_cast<long double>(yCount + 2 * uvCount);
    long double totalError =
        static_cast<long double>(ySSD) + static_cast<long double>(uSSD) + static_cast<long double>(vSSD);
    double avgPSNR;
    if (totalError == 0.0L) {
        avgPSNR = std::numeric_limits<double>::infinity();
    } else {
        long double globalMSE = totalError / totalSamples;
        avgPSNR = 10.0 * std::log10((maxSampleValue * maxSampleValue) / static_cast<double>(globalMSE));
    }

    PSNRResult result(avgPSNR, yPSNR, uPSNR, vPSNR);
    return result;
}

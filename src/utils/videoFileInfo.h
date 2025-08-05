#pragma once

#include <QString>
#include "ui/videoWindow.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

struct VideoFileInfo {
    QString filename;
    int width;
    int height;
    double framerate;
    AVPixelFormat pixelFormat;
    QRhi::Implementation graphicsApi;
    VideoWindow* windowPtr;
    bool forceSoftwareDecoding = false;
};
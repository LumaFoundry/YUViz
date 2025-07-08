#pragma once

#include <QString>
#include "ui/videoWindow.h"

struct VideoFileInfo {
    QString filename;
    int width;
    int height;
    double framerate;
    QRhi::Implementation graphicsApi;
    VideoWindow* windowPtr;
};
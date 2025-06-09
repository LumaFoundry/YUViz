#include "frameData.h"


FrameData::FrameData() {
    // Assume default format is V8P4 (8-bit planar 4:2:0)
    resize(QSize(1, 1), V8P4);
    yData.fill(16, 1);
    uData.fill(128, 1);
    vData.fill(128, 1);
    renderFormat = diskFormat = V8P4;
}

void FrameData::resize(const QSize &size, DataFormat f) {

    diskFormat = renderFormat = f;
    ySize = size;

    // Calculate the size of the chroma planes based on the format
    switch (f) {
    case V8P4:
    case V16P4:
        cSize = ySize;
        break;
    case V8P2:
    case V16P2:
        cSize = QSize(size.width() / 2, size.height());
        break;
    case V8P0:
    case V16P0:
    case YV12:
        cSize = QSize(size.width() / 2, size.height() / 2);
        break;
    case UYVY:
    case V216:
    case V210:
        isPlanar = false;
        cSize = ySize;
        break;
    
    default:
        break;
    }

    // Calculate bytes per pixel based on the format
    // TODO: handle V210 format properly
    int bytesPerSample = (f == V16P4 || f == V16P2 || f == V16P0 || f == V216) ? 2 : 1;

    // Resize the data buffers accordingly
    yData.resize(size.width() * size.height() * bytesPerSample);
    if (isPlanar) {
        uData.resize(cSize.width() * cSize.height() * bytesPerSample);
        vData.resize(cSize.width() * cSize.height() * bytesPerSample);
    } else {
        uData.clear();
        vData.clear();
    }
}



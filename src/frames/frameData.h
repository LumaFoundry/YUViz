#pragma once

#include <QScopedPointer>
#include <QByteArray>
#include <QSize>

class FrameData {

public:

	enum DataFormat {
		V8P0 = 0,   // 8-bit   planar 4:2:0
		V8P2,       // 8-bit   planar 4:2:2
		V8P4,       // 8-bit   planar 4:4:4
		YV12,       // 8-bit   planar 4:2:0
		V16P0,      // 16-bit  planar 4:2:0
		V16P2,      // 16-bit  planar 4:2:2
		V16P4,      // 16-bit  planar 4:4:4
		UYVY,       // 8-bit   packed 4:2:2
		V216,       // 16-bit  packed 4:4:4
		V210,       // 10-bit  packed 4:2:2
		Unknown
	};

    DataFormat diskFormat;
    DataFormat renderFormat;

    // Index of the current frame
    uint64_t frameIndex;

    FrameData();
    
    void resize(const QSize &size, DataFormat f); // allocate or resize plane buffers
    // Getters for raw pixel buffers
    const QByteArray &getYData() const { return yData; }
    const QByteArray &getUData() const { return uData; }
    const QByteArray &getVData() const { return vData; }
    bool isPlanar = true;

    // Getters for Y and Chroma sizes
    QSize getYSize() const { return ySize; }
    QSize getCSize() const { return cSize; }


private:
    QSize ySize;
    QSize cSize;
    
    QByteArray yData;
    QByteArray uData;
    QByteArray vData;

};


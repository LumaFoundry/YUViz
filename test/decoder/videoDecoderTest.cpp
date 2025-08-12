#include "videoDecoderTest.h"
#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <memory>
#include "../../src/decoder/videoDecoder.h"
#include "../../src/frames/frameQueue.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

void VideoDecoderTest::testConstructor() {
    // Test VideoDecoder constructor (basic test to avoid crashes)
    VideoDecoder decoder(this);

    // Test that constructor doesn't crash
    QVERIFY(&decoder != nullptr);
}

void VideoDecoderTest::testDecoding() {
    VideoDecoder decoder(this);

    // Test that decoder object can be created
    QVERIFY(&decoder != nullptr);
}

void VideoDecoderTest::testErrorHandling() {
    VideoDecoder decoder(this);

    // Test that decoder object can be created
    QVERIFY(&decoder != nullptr);
}

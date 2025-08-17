#include <QtTest>
#include "frames/frameMeta.h"

class FrameMetaTest : public QObject {
    Q_OBJECT
  private slots:
    void testSettersAndGetters();
};

void FrameMetaTest::testSettersAndGetters() {
    FrameMeta meta;
    meta.setYWidth(1920);
    meta.setYHeight(1080);
    meta.setUVWidth(960);
    meta.setUVHeight(540);
    meta.setTotalFrames(100);
    meta.setDuration(123456);
    meta.setFilename("test.yuv");
    meta.setCodecName("h264");
    meta.setPixelFormat(AV_PIX_FMT_YUV420P);
    AVRational tb = {1, 30};
    meta.setTimeBase(tb);
    AVRational sar = {4, 3};
    meta.setSampleAspectRatio(sar);
    meta.setColorRange(AVCOL_RANGE_JPEG);
    meta.setColorSpace(AVCOL_SPC_BT709);

    QCOMPARE(meta.yWidth(), 1920);
    QCOMPARE(meta.yHeight(), 1080);
    QCOMPARE(meta.uvWidth(), 960);
    QCOMPARE(meta.uvHeight(), 540);
    QCOMPARE(meta.ySize(), 1920 * 1080);
    QCOMPARE(meta.uvSize(), 960 * 540);
    QCOMPARE(meta.totalFrames(), 100);
    QCOMPARE(meta.duration(), int64_t(123456));
    QCOMPARE(meta.filename(), std::string("test.yuv"));
    QCOMPARE(meta.codecName(), std::string("h264"));
    QCOMPARE(meta.format(), AV_PIX_FMT_YUV420P);
    QCOMPARE(meta.timeBase().num, 1);
    QCOMPARE(meta.timeBase().den, 30);
    QCOMPARE(meta.sampleAspectRatio().num, 4);
    QCOMPARE(meta.sampleAspectRatio().den, 3);
    QCOMPARE(meta.colorRange(), AVCOL_RANGE_JPEG);
    QCOMPARE(meta.colorSpace(), AVCOL_SPC_BT709);
}

QTEST_MAIN(FrameMetaTest)
#include "test_framemeta.moc"

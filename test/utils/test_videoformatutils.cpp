#include <QtTest>
#include "utils/videoFormatUtils.h"

class TestVideoFormatUtils : public QObject {
    Q_OBJECT
  private slots:
    void testDetectFormatFromExtension();
    void testStringToPixelFormatAndBack();
    void testSupportedFormatsAndExtensions();
    void testIsValidFormat();
    void testIsCompressedFormat();
    void testGetFormatType();
    void testGetFormatByIdentifier();
    void testGetFormatIdentifiersAndDisplayNames();
};

void TestVideoFormatUtils::testDetectFormatFromExtension() {
    QCOMPARE(VideoFormatUtils::detectFormatFromExtension("foo.y4m"), QString("Y4M"));
    QCOMPARE(VideoFormatUtils::detectFormatFromExtension("bar.420p.yuv"), QString("420P"));
    QCOMPARE(VideoFormatUtils::detectFormatFromExtension("baz.422p.raw"), QString("422P"));
    QCOMPARE(VideoFormatUtils::detectFormatFromExtension("qux.444p.yuv"), QString("444P"));
    QCOMPARE(VideoFormatUtils::detectFormatFromExtension("qux.nv12"), QString("NV12"));
    QCOMPARE(VideoFormatUtils::detectFormatFromExtension("test.nv21"), QString("NV21"));
    QCOMPARE(VideoFormatUtils::detectFormatFromExtension("test.yuyv"), QString("YUYV"));
    QCOMPARE(VideoFormatUtils::detectFormatFromExtension("test.uyvy"), QString("UYVY"));
    QCOMPARE(VideoFormatUtils::detectFormatFromExtension("unknown.mp4"), QString("COMPRESSED"));
}

void TestVideoFormatUtils::testStringToPixelFormatAndBack() {
    QCOMPARE(VideoFormatUtils::stringToPixelFormat("420P"), AV_PIX_FMT_YUV420P);
    QCOMPARE(VideoFormatUtils::stringToPixelFormat("422P"), AV_PIX_FMT_YUV422P);
    QCOMPARE(VideoFormatUtils::stringToPixelFormat("444P"), AV_PIX_FMT_YUV444P);
    QCOMPARE(VideoFormatUtils::stringToPixelFormat("YUYV"), AV_PIX_FMT_YUYV422);
    QCOMPARE(VideoFormatUtils::stringToPixelFormat("UYVY"), AV_PIX_FMT_UYVY422);
    QCOMPARE(VideoFormatUtils::stringToPixelFormat("NV12"), AV_PIX_FMT_NV12);
    QCOMPARE(VideoFormatUtils::stringToPixelFormat("NV21"), AV_PIX_FMT_NV21);
    QCOMPARE(VideoFormatUtils::stringToPixelFormat("INVALID"), AV_PIX_FMT_NONE);

    QCOMPARE(VideoFormatUtils::pixelFormatToString(AV_PIX_FMT_YUV420P), QString("420P"));
    QCOMPARE(VideoFormatUtils::pixelFormatToString(AV_PIX_FMT_YUV422P), QString("422P"));
    QCOMPARE(VideoFormatUtils::pixelFormatToString(AV_PIX_FMT_YUV444P), QString("444P"));
    QCOMPARE(VideoFormatUtils::pixelFormatToString(AV_PIX_FMT_YUYV422), QString("YUYV"));
    QCOMPARE(VideoFormatUtils::pixelFormatToString(AV_PIX_FMT_UYVY422), QString("UYVY"));
    QCOMPARE(VideoFormatUtils::pixelFormatToString(AV_PIX_FMT_NV12), QString("NV12"));
    QCOMPARE(VideoFormatUtils::pixelFormatToString(AV_PIX_FMT_NV21), QString("NV21"));
}

void TestVideoFormatUtils::testSupportedFormatsAndExtensions() {
    auto formats = VideoFormatUtils::getSupportedFormats();
    QVERIFY(!formats.isEmpty());
    auto rawExts = VideoFormatUtils::getRawVideoExtensions();
    QVERIFY(rawExts.contains(".yuv"));
    auto allExts = VideoFormatUtils::getAllSupportedExtensions();
    QVERIFY(allExts.contains(".y4m"));
}

void TestVideoFormatUtils::testIsValidFormat() {
    QVERIFY(VideoFormatUtils::isValidFormat("420P"));
    QVERIFY(VideoFormatUtils::isValidFormat("422P"));
    QVERIFY(VideoFormatUtils::isValidFormat("YUYV"));
    QVERIFY(!VideoFormatUtils::isValidFormat("INVALID"));
}

void TestVideoFormatUtils::testIsCompressedFormat() {
    QVERIFY(VideoFormatUtils::isCompressedFormat("COMPRESSED"));
    QVERIFY(VideoFormatUtils::isCompressedFormat("Y4M"));
    QVERIFY(!VideoFormatUtils::isCompressedFormat("420P"));
}

void TestVideoFormatUtils::testGetFormatType() {
    QCOMPARE(VideoFormatUtils::getFormatType("420P"), FormatType::RAW_YUV);
    QCOMPARE(VideoFormatUtils::getFormatType("Y4M"), FormatType::Y4M);
    QCOMPARE(VideoFormatUtils::getFormatType("COMPRESSED"), FormatType::COMPRESSED);
    QCOMPARE(VideoFormatUtils::getFormatType("INVALID"), FormatType::COMPRESSED); // Default
}

void TestVideoFormatUtils::testGetFormatByIdentifier() {
    auto fmt = VideoFormatUtils::getFormatByIdentifier("420P");
    QCOMPARE(fmt.identifier, QString("420P"));
    QCOMPARE(fmt.pixelFormat, AV_PIX_FMT_YUV420P);
    auto invalid = VideoFormatUtils::getFormatByIdentifier("INVALID");
    QCOMPARE(invalid.identifier, QString(""));
    QCOMPARE(invalid.pixelFormat, AV_PIX_FMT_NONE);
}

void TestVideoFormatUtils::testGetFormatIdentifiersAndDisplayNames() {
    auto identifiers = VideoFormatUtils::getFormatIdentifiers();
    auto displayNames = VideoFormatUtils::getDisplayNames();
    QVERIFY(identifiers.contains("420P"));
    QVERIFY(identifiers.contains("COMPRESSED"));
    QVERIFY(displayNames.contains("420P - YUV420P (Planar)"));
    QVERIFY(displayNames.contains("Compressed Video"));
}

QTEST_MAIN(TestVideoFormatUtils)
#include "test_videoformatutils.moc"

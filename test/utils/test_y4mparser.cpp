#include <QTemporaryFile>
#include <QtTest>
#include "utils/y4mParser.h"

class TestY4MParser : public QObject {
    Q_OBJECT
  private slots:
    void testParseValidHeader();
    void testParseInvalidHeader();
    void testIsY4MFile();
    void testColorSpaceToPixelFormat();
    void testCalculateFrameSizeAndTotalFrames();
};

void TestY4MParser::testParseValidHeader() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray header = "YUV4MPEG2 W1920 H1080 F25:1 I? A0:0 C420\nFRAME\n";
    tempFile.write(header);
    tempFile.flush();
    Y4MInfo info = Y4MParser::parseHeader(tempFile.fileName());
    QVERIFY(info.isValid);
    QCOMPARE(info.width, 1920);
    QCOMPARE(info.height, 1080);
    QCOMPARE(info.frameRate, 25.0);
    QCOMPARE(info.colorSpace, QString("420"));
    QVERIFY(info.headerSize > 0);
}

void TestY4MParser::testParseInvalidHeader() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray header = "NOTYUV4MPEG2 W0 H0\n";
    tempFile.write(header);
    tempFile.flush();
    Y4MInfo info = Y4MParser::parseHeader(tempFile.fileName());
    QVERIFY(!info.isValid);
}

void TestY4MParser::testIsY4MFile() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray header = "YUV4MPEG2 W640 H480\n";
    tempFile.write(header);
    tempFile.flush();
    QVERIFY(Y4MParser::isY4MFile(tempFile.fileName()));

    QTemporaryFile tempFile2;
    QVERIFY(tempFile2.open());
    QByteArray badHeader = "BADHEADER W640 H480\n";
    tempFile2.write(badHeader);
    tempFile2.flush();
    QVERIFY(!Y4MParser::isY4MFile(tempFile2.fileName()));
}

void TestY4MParser::testColorSpaceToPixelFormat() {
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("420"), AV_PIX_FMT_YUV420P);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("420jpeg"), AV_PIX_FMT_YUV420P);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("420paldv"), AV_PIX_FMT_YUV420P);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("420mpeg2"), AV_PIX_FMT_YUV420P);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("422"), AV_PIX_FMT_YUV422P);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("444"), AV_PIX_FMT_YUV444P);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("411"), AV_PIX_FMT_YUV411P);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("mono"), AV_PIX_FMT_GRAY8);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("420p10"), AV_PIX_FMT_YUV420P10LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("422p10"), AV_PIX_FMT_YUV422P10LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("444p10"), AV_PIX_FMT_YUV444P10LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("420p12"), AV_PIX_FMT_YUV420P12LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("422p12"), AV_PIX_FMT_YUV422P12LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("444p12"), AV_PIX_FMT_YUV444P12LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("420p14"), AV_PIX_FMT_YUV420P14LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("422p14"), AV_PIX_FMT_YUV422P14LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("444p14"), AV_PIX_FMT_YUV444P14LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("420p16"), AV_PIX_FMT_YUV420P16LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("422p16"), AV_PIX_FMT_YUV422P16LE);
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("444p16"), AV_PIX_FMT_YUV444P16LE);
    // Unknown color space should default to YUV420P
    QCOMPARE(Y4MParser::colorSpaceToPixelFormat("unknown"), AV_PIX_FMT_YUV420P);
}

void TestY4MParser::testCalculateFrameSizeAndTotalFrames() {
    Y4MInfo info;
    info.width = 4;
    info.height = 2;
    info.headerSize = 20; // arbitrary
    info.isValid = true;

    // YUV420P
    info.pixelFormat = AV_PIX_FMT_YUV420P;
    int frameSize420 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize420, 12);

    // YUV422P: Y = 8, U = ((4+1)/2)*2=2*2=4, V = same as U, total = 8+4+4=16
    info.pixelFormat = AV_PIX_FMT_YUV422P;
    int frameSize422 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize422, 16);

    // YUV444P: Y = 8, U = 8, V = 8, total = 24
    info.pixelFormat = AV_PIX_FMT_YUV444P;
    int frameSize444 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize444, 24);

    // YUV411P: Y = 8, U = ((4+3)/4)*2=1*2=2, V = same as U, total = 8+2+2=12
    info.pixelFormat = AV_PIX_FMT_YUV411P;
    int frameSize411 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize411, 12);

    // GRAY8: Y = 8
    info.pixelFormat = AV_PIX_FMT_GRAY8;
    int frameSizeGray = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSizeGray, 8);

    // YUV420P10LE: Y = 8*2=16, U = 2*1*2=4, V = same as U, total = 16+4+4=24
    info.pixelFormat = AV_PIX_FMT_YUV420P10LE;
    int frameSize420p10 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize420p10, 24);

    // YUV422P10LE: Y = 8*2=16, U = 2*2*2=8, V = same as U, total = 16+8+8=32
    info.pixelFormat = AV_PIX_FMT_YUV422P10LE;
    int frameSize422p10 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize422p10, 32);

    // YUV444P10LE: YUV444P10LE = width*height*6 = 4*2*6 = 48
    info.pixelFormat = AV_PIX_FMT_YUV444P10LE;
    int frameSize444p10 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize444p10, 48);

    // YUV420P12LE: Y = 8*2=16, U = 2*1*2=4, V = same as U, total = 16+4+4=24
    info.pixelFormat = AV_PIX_FMT_YUV420P12LE;
    int frameSize420p12 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize420p12, 24);

    // YUV422P12LE: Y = 8*2=16, U = 2*2*2=8, V = same as U, total = 16+8+8=32
    info.pixelFormat = AV_PIX_FMT_YUV422P12LE;
    int frameSize422p12 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize422p12, 32);

    // YUV444P12LE: width*height*6 = 4*2*6 = 48
    info.pixelFormat = AV_PIX_FMT_YUV444P12LE;
    int frameSize444p12 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize444p12, 48);

    // YUV420P14LE: Y = 8*2=16, U = 2*1*2=4, V = same as U, total = 16+4+4=24
    info.pixelFormat = AV_PIX_FMT_YUV420P14LE;
    int frameSize420p14 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize420p14, 24);

    // YUV422P14LE: Y = 8*2=16, U = 2*2*2=8, V = same as U, total = 16+8+8=32
    info.pixelFormat = AV_PIX_FMT_YUV422P14LE;
    int frameSize422p14 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize422p14, 32);

    // YUV444P14LE: width*height*6 = 4*2*6 = 48
    info.pixelFormat = AV_PIX_FMT_YUV444P14LE;
    int frameSize444p14 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize444p14, 48);

    // YUV420P16LE: Y = 8*2=16, U = 2*1*2=4, V = same as U, total = 16+4+4=24
    info.pixelFormat = AV_PIX_FMT_YUV420P16LE;
    int frameSize420p16 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize420p16, 24);

    // YUV422P16LE: Y = 8*2=16, U = 2*2*2=8, V = same as U, total = 16+8+8=32
    info.pixelFormat = AV_PIX_FMT_YUV422P16LE;
    int frameSize422p16 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize422p16, 32);

    // YUV444P16LE: width*height*6 = 4*2*6 = 48
    info.pixelFormat = AV_PIX_FMT_YUV444P16LE;
    int frameSize444p16 = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSize444p16, 48);

    // Default branch (should match YUV420P)
    info.pixelFormat = (AVPixelFormat)-1;
    int frameSizeDefault = Y4MParser::calculateFrameSize(info);
    QCOMPARE(frameSizeDefault, 12);

    // Create a temp file with header and 3 frames
    info.pixelFormat = AV_PIX_FMT_YUV420P;
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray header(20, 'H'); // header size = 20
    tempFile.write(header);
    QByteArray framePrefix = "FRAME\n";
    QByteArray frameData(12, 'F');
    for (int i = 0; i < 3; ++i) {
        tempFile.write(framePrefix);
        tempFile.write(frameData);
    }
    tempFile.flush();
    int totalFrames = Y4MParser::calculateTotalFrames(tempFile.fileName(), info);
    QCOMPARE(totalFrames, 3);

    // Invalid info should return -1
    Y4MInfo invalidInfo;
    invalidInfo.isValid = false;
    QCOMPARE(Y4MParser::calculateFrameSize(invalidInfo), 0);
    QCOMPARE(Y4MParser::calculateTotalFrames(tempFile.fileName(), invalidInfo), -1);
}

QTEST_MAIN(TestY4MParser)
#include "test_y4mparser.moc"

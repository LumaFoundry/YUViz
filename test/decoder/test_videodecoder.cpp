#include <QObject>
#include <QTemporaryFile>
#include <QtTest/QTest>
#include <QtTest>
#include "decoder/videoDecoder.h"
#include "frames/frameQueue.h"

class TestVideoDecoder : public QObject {
    Q_OBJECT
  private slots:
    void testOpenAndLoadFramesYUV();
    void testOpenAndLoadFramesY4M();
    void testOpenAndLoadFramesMP4();
    void testOpenAndLoadFramesPackedYUV();
    void testOpenAndLoadFramesSemiPlanarYUV();
    void testOpenAndLoadFramesNV21SemiPlanarYUV();
    void testOpenAndLoadFramesUYVYPackedYUV();

  private:
    void runDecoderTest(const QString& filePath,
                        AVPixelFormat format,
                        int width,
                        int height,
                        const QString& expectedCodecName,
                        bool setFormatAndDims = true);

  private:
    std::shared_ptr<FrameQueue> frameQueue;
    std::shared_ptr<FrameMeta> meta;
};

void TestVideoDecoder::runDecoderTest(const QString& filePath,
                                      AVPixelFormat format,
                                      int width,
                                      int height,
                                      const QString& expectedCodecName,
                                      bool setFormatAndDims) {
    QVERIFY(QFile::exists(filePath));

    VideoDecoder decoder;
    decoder.setFileName(filePath.toStdString());
    if (setFormatAndDims && format != AV_PIX_FMT_NONE) {
        decoder.setDimensions(width, height);
        decoder.setFormat(format);
    }
    decoder.openFile();
    auto meta = std::make_shared<FrameMeta>(decoder.getMetaData());
    auto frameQueue = std::make_shared<FrameQueue>(meta, 25);
    decoder.setFrameQueue(frameQueue);

    QVERIFY(meta->totalFrames() > 0);
    QVERIFY(meta->yWidth() == width);
    QVERIFY(meta->yHeight() == height);
    if (!expectedCodecName.isEmpty())
        QVERIFY(QString::fromStdString(meta->codecName()) == expectedCodecName);

    // Load 2 frames forward
    QSignalSpy spy(&decoder, SIGNAL(framesLoaded(bool)));
    decoder.loadFrames(2, 1);
    QElapsedTimer timer;
    timer.start();
    while (spy.count() == 0 && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    QVERIFY(spy.count() == 1);

    // Load 1 frame backward
    QSignalSpy spyRev(&decoder, SIGNAL(framesLoaded(bool)));
    decoder.loadFrames(1, -1);
    QElapsedTimer timerRev;
    timerRev.start();
    while (spyRev.count() == 0 && timerRev.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    QVERIFY(spyRev.count() == 1);

    // Test seeking to frame 1
    QSignalSpy seekSpy(&decoder, SIGNAL(frameSeeked(qint64)));
    decoder.seek(3);
    QElapsedTimer seekTimer;
    seekTimer.start();
    while (seekSpy.count() == 0 && seekTimer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    QVERIFY(seekSpy.count() == 1);

    // load a frame after seeking and verify framesLoaded signal
    QSignalSpy spyAfterSeek(&decoder, SIGNAL(framesLoaded(bool)));
    decoder.loadFrames(1, -1);
    QElapsedTimer timerAfterSeek;
    timerAfterSeek.start();
    while (spyAfterSeek.count() == 0 && timerAfterSeek.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    QVERIFY(spyAfterSeek.count() == 1);
}

void TestVideoDecoder::testOpenAndLoadFramesYUV() {
    QString yuvPath = QFileInfo(__FILE__).absolutePath() + "/../resources/test_stub.yuv";
    runDecoderTest(yuvPath, AV_PIX_FMT_YUV420P, 2, 2, "");
}

void TestVideoDecoder::testOpenAndLoadFramesY4M() {
    QString y4mPath = QFileInfo(__FILE__).absolutePath() + "/../resources/test_stub.y4m";
    runDecoderTest(y4mPath, AV_PIX_FMT_NONE, 2, 2, "Y4M", false);
}

void TestVideoDecoder::testOpenAndLoadFramesMP4() {
    QString sourceDir = QFileInfo(__FILE__).absolutePath();
    QString mp4Path = QDir(sourceDir).filePath("../resources/test_stub.mp4");
    if (!QFile::exists(mp4Path)) {
        mp4Path = QDir(sourceDir).filePath("resources/test_stub.mp4");
    }
    runDecoderTest(mp4Path, AV_PIX_FMT_NONE, 2, 2, "", false);
}

void TestVideoDecoder::testOpenAndLoadFramesPackedYUV() {
    QString packedPath = QFileInfo(__FILE__).absolutePath() + "/../resources/test_stub_yuyv422.yuv";
    runDecoderTest(packedPath, AV_PIX_FMT_YUYV422, 24, 2, "");
}

void TestVideoDecoder::testOpenAndLoadFramesSemiPlanarYUV() {
    QString semiPlanarPath = QFileInfo(__FILE__).absolutePath() + "/../resources/test_stub_nv12.yuv";
    runDecoderTest(semiPlanarPath, AV_PIX_FMT_NV12, 32, 2, "");
}

void TestVideoDecoder::testOpenAndLoadFramesNV21SemiPlanarYUV() {
    QString nv21Path = QFileInfo(__FILE__).absolutePath() + "/../resources/test_stub_nv21.yuv";
    runDecoderTest(nv21Path, AV_PIX_FMT_NV21, 32, 2, "");
}

void TestVideoDecoder::testOpenAndLoadFramesUYVYPackedYUV() {
    QString uyvyPath = QFileInfo(__FILE__).absolutePath() + "/../resources/test_stub_uyvy422.yuv";
    runDecoderTest(uyvyPath, AV_PIX_FMT_UYVY422, 24, 2, "");
}

QTEST_MAIN(TestVideoDecoder)
#include "test_videodecoder.moc"

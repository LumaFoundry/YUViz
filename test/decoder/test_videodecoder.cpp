#include <QObject>
#include <QTemporaryFile>
#include <QtTest>
#include "decoder/videoDecoder.h"
#include "frames/frameQueue.h"

class TestVideoDecoder : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase();
    void cleanupTestCase();
    void testOpenStubYUVFile();

  private:
    QTemporaryFile* stubFile;
    std::shared_ptr<FrameQueue> frameQueue;
    std::shared_ptr<FrameMeta> meta;
};

void TestVideoDecoder::initTestCase() {
    // Create a minimal stub YUV file (e.g., 2x2 YUV420P, 1 frame)
    stubFile = new QTemporaryFile(QDir::temp().absoluteFilePath("stub_yuvXXXXXX.yuv"));
    QVERIFY(stubFile->open());
    QByteArray yuvData;
    // Y plane (2x2)
    yuvData.append(QByteArray::fromRawData("\x10\x20\x30\x40", 4));
    // U plane (1x1)
    yuvData.append(QByteArray::fromRawData("\x50", 1));
    // V plane (1x1)
    yuvData.append(QByteArray::fromRawData("\x60", 1));
    stubFile->write(yuvData);
    stubFile->flush();
    meta = std::make_shared<FrameMeta>();
    frameQueue = std::make_shared<FrameQueue>(meta, 8); // Small queue for test
}

void TestVideoDecoder::cleanupTestCase() {
    stubFile->close();
    delete stubFile;
}

void TestVideoDecoder::testOpenStubYUVFile() {
    VideoDecoder decoder;
    decoder.setFileName(stubFile->fileName().toStdString());
    decoder.setDimensions(2, 2);
    decoder.setFormat(AV_PIX_FMT_YUV420P);
    decoder.setFrameQueue(frameQueue);
    decoder.openFile();
    FrameMeta meta = decoder.getMetaData();
    QCOMPARE(meta.yWidth(), 2);
    QCOMPARE(meta.yHeight(), 2);
    QCOMPARE(meta.format(), AV_PIX_FMT_YUV420P);
    QVERIFY(meta.totalFrames() >= 1);
}

QTEST_MAIN(TestVideoDecoder)
#include "test_videodecoder.moc"

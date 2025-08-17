#include <QtTest>
#include <memory>
#include <vector>
#include "frames/frameData.h"

class FrameDataTest : public QObject {
    Q_OBJECT

  private slots:
    void testPlanePointers();
    void testPtsAndEndFrame();
    void testEndFrameSetterGetter();
};

void FrameDataTest::testPlanePointers() {
    int ySize = 4;
    int uvSize = 2;
    auto buffer = std::make_shared<std::vector<uint8_t>>(10, 0xFF);
    size_t offset = 1;
    FrameData frame(ySize, uvSize, buffer, offset);

    QVERIFY(frame.yPtr() == buffer->data() + offset + 0);
    QVERIFY(frame.uPtr() == buffer->data() + offset + ySize);
    QVERIFY(frame.vPtr() == buffer->data() + offset + ySize + uvSize);
}

void FrameDataTest::testPtsAndEndFrame() {
    FrameData frame(4, 2, std::make_shared<std::vector<uint8_t>>(10), 0);
    frame.setPts(12345);
    QVERIFY(frame.pts() == 12345);
    QVERIFY(!frame.isEndFrame());
    frame.setEndFrame(true);
    QVERIFY(frame.isEndFrame());
    frame.setEndFrame(false);
    QVERIFY(!frame.isEndFrame());
}

void FrameDataTest::testEndFrameSetterGetter() {
    FrameData frame(4, 2, std::make_shared<std::vector<uint8_t>>(10), 0);
    QVERIFY(!frame.isEndFrame());
    frame.setEndFrame(true);
    QVERIFY(frame.isEndFrame());
    frame.setEndFrame(false);
    QVERIFY(!frame.isEndFrame());
}

QTEST_MAIN(FrameDataTest)
#include "test_framedata.moc"

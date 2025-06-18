#include <QtTest>
#include <set>
#include "frames/frameQueue.h"
#include "frames/frameMeta.h"

class FrameQueueTest : public QObject {
    Q_OBJECT

private slots:
    void testFifoOrder();
    void testReusability();
};

void FrameQueueTest::testFifoOrder() {
    FrameMeta meta;
    meta.setYWidth(2);
    meta.setYHeight(2);
    meta.setUVWidth(1);
    meta.setUVHeight(1);
    FrameQueue queue(meta);

    // Write PTS values to 10 frames
    for (int i = 0; i < 10; ++i) {
        auto* frame = queue.getTailFrame();
        frame->setPts(i);
        queue.incrementTail();
    }

    // Read back and check order
    for (int i = 0; i < 10; ++i) {
        auto* frame = queue.getHeadFrame();
        QCOMPARE(frame->pts(), int64_t(i));
        queue.incrementHead();
    }
}

void FrameQueueTest::testReusability() {
    FrameMeta meta;
    meta.setYWidth(2);
    meta.setYHeight(2);
    meta.setUVWidth(1);
    meta.setUVHeight(1);
    FrameQueue queue(meta);

    std::set<void*> framePtrs;

    for (int i = 0; i < 100; ++i) {
        auto* f = queue.getTailFrame();
        framePtrs.insert(static_cast<void*>(f));
        queue.incrementTail();
        queue.getHeadFrame();
        queue.incrementHead();
    }

    // Should not exceed queueSize
    QVERIFY(framePtrs.size() <= 50); 
}

QTEST_MAIN(FrameQueueTest)
#include "test_framequeue.moc"

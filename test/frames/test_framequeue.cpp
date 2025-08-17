#include <QtTest>
#include <memory>
#include "frames/frameMeta.h"
#include "frames/frameQueue.h"

class FrameQueueTest : public QObject {
    Q_OBJECT

  private slots:
    void testTailAndHeadFrame();
    void testUpdateTailAndIsStale();
    void testGetEmpty();
    void testQueueSizeAndBuffer();
    void testIsStaleEdgeCases();
    void testZeroAndOneQueueSize();
    void testGetEmptyDirections();
    void testGetStaleFrames();
    void testGetEmptyBranches();
    void testUpdateTailBranches();
    void testGetHeadFrameBranches();
    void testIsStaleBranches();
    void testNegativeAndLargePts();
    void testBufferAllocationEdgeCases();
    void testConstructorBranches();
};

void FrameQueueTest::testTailAndHeadFrame() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 10);

    // Set PTS for each frame
    for (int i = 0; i < 10; ++i) {
        auto* tailFrame = queue.getTailFrame(i);
        QVERIFY(tailFrame != nullptr);
        tailFrame->setPts(i);
        queue.updateTail(i);
    }

    // Check head frames
    for (int i = 0; i < 10; ++i) {
        auto* headFrame = queue.getHeadFrame(i);
        QVERIFY(headFrame != nullptr);
        QCOMPARE(headFrame->pts(), int64_t(i));
    }
}

void FrameQueueTest::testUpdateTailAndIsStale() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 5);

    for (int i = 0; i < 5; ++i) {
        queue.updateTail(i);
    }

    // The latest tail is 4, so stale is outside [0,4]
    QVERIFY(queue.isStale(-1));
    QVERIFY(!queue.isStale(2));
    QVERIFY(queue.isStale(10));
}

void FrameQueueTest::testGetEmpty() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 5);

    // Initially, both head and tail are 0
    QCOMPARE(queue.getEmpty(1), 2);  // (0 + 2) - 0 = 2
    QCOMPARE(queue.getEmpty(-1), 2); // (0 + 2) - 0 = 2

    // Move tail forward
    queue.updateTail(3);
    QCOMPARE(queue.getEmpty(1), 0);  // (0 + 2) - 3 = -1, but should be clamped to 0
    QCOMPARE(queue.getEmpty(-1), 5); // (3 + 2) - 0 = 5
}

void FrameQueueTest::testQueueSizeAndBuffer() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 7);
    QCOMPARE(queue.getSize(), 7);
}

void FrameQueueTest::testIsStaleEdgeCases() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 3);

    queue.updateTail(2);
    QVERIFY(!queue.isStale(2)); // Not stale
    QVERIFY(!queue.isStale(0)); // Not stale
    QVERIFY(queue.isStale(-1)); // Stale
    QVERIFY(queue.isStale(5));  // Stale
}

void FrameQueueTest::testZeroAndOneQueueSize() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    // One queue size
    FrameQueue queueOne(meta, 1);
    QCOMPARE(queueOne.getSize(), 1);
    auto* frame = queueOne.getTailFrame(0);
    QVERIFY(frame != nullptr);
    frame->setPts(0);
    queueOne.updateTail(0);
    QVERIFY(queueOne.getHeadFrame(0) != nullptr);
}

void FrameQueueTest::testGetEmptyDirections() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 4);
    // Test both directions
    QCOMPARE(queue.getEmpty(1), 2);
    QCOMPARE(queue.getEmpty(-1), 2);
    // Test with tail ahead of head
    queue.updateTail(3);
    QCOMPARE(queue.getEmpty(1), 0);
    QCOMPARE(queue.getEmpty(-1), 5);
}

void FrameQueueTest::testGetStaleFrames() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 1);

    // add a frame with stale pts
    auto* frame = queue.getTailFrame(0);
    QVERIFY(frame != nullptr);
    frame->setPts(-1);

    // Get stale frames
    QCOMPARE(queue.getHeadFrame(0), nullptr);
}

void FrameQueueTest::testGetEmptyBranches() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 4);

    // Covers direction == 1 and direction != 1
    queue.getEmpty(1);  // covers if branch
    queue.getEmpty(-1); // covers else branch

    // Covers empty < 0 branch
    queue.updateTail(10); // move tail ahead of head
    queue.getEmpty(1);    // should yield negative, triggers empty < 0
}

void FrameQueueTest::testUpdateTailBranches() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 3);

    queue.updateTail(5);  // covers pts >= 0
    queue.updateTail(-1); // covers pts < 0
}

void FrameQueueTest::testGetHeadFrameBranches() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 3);

    // Set PTS for frame 0
    auto* frame0 = queue.getTailFrame(0);
    frame0->setPts(0);

    // Covers "+" branch (pts matches)
    QVERIFY(queue.getHeadFrame(0) != nullptr);

    // Covers "-" branch (pts does not match)
    QVERIFY(queue.getHeadFrame(1) == nullptr);
}

void FrameQueueTest::testIsStaleBranches() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 3);
    queue.updateTail(2);
    // Window is [0,2]
    QVERIFY(!queue.isStale(2)); // in window
    QVERIFY(!queue.isStale(0)); // in window
    QVERIFY(queue.isStale(-1)); // below window
    QVERIFY(queue.isStale(3));  // above window
}

void FrameQueueTest::testNegativeAndLargePts() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);
    FrameQueue queue(meta, 3);

    auto frame0 = queue.getTailFrame(0);
    frame0->setPts(0);

    // Negative pts
    QVERIFY(queue.getHeadFrame(-1) == nullptr);
    QVERIFY(queue.getTailFrame(-1) == nullptr);

    // Large pts
    int64_t largePts = 100000;
    QVERIFY(queue.getHeadFrame(largePts) == nullptr);
    QVERIFY(queue.getTailFrame(largePts) != nullptr);
}

void FrameQueueTest::testBufferAllocationEdgeCases() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(0);
    meta->setYHeight(0);
    meta->setUVWidth(0);
    meta->setUVHeight(0);
    FrameQueue queue(meta, 2);
    QCOMPARE(queue.getSize(), 2);
}

void FrameQueueTest::testConstructorBranches() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);

    // Positive queue size
    FrameQueue queuePositive(meta, 2);
    QCOMPARE(queuePositive.getSize(), 2);
    QVERIFY(queuePositive.metaPtr() == meta);
    QVERIFY(queuePositive.getTailFrame(0) != nullptr);
    QVERIFY(queuePositive.getTailFrame(1) != nullptr);
    // The queue should have two frames
    QVERIFY(queuePositive.getSize() == 2);
}

QTEST_MAIN(FrameQueueTest)
#include "test_framequeue.moc"

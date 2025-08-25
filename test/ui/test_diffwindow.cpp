#include <QtTest>
#include "rendering/diffRenderer.h"
#include "ui/diffWindow.h"

// Stub DiffRenderer for safe unit testing
class StubDiffRenderer : public DiffRenderer {
  public:
    StubDiffRenderer(QQuickItem* parent = nullptr, std::shared_ptr<FrameMeta> metaPtr = nullptr) :
        DiffRenderer(parent, metaPtr ? metaPtr : std::make_shared<FrameMeta>()) {}

    void uploadFrame(FrameData*, FrameData*) override {}
    void releaseBatch() override {}
    void setDiffConfig(int, float, int) override {}
    void setMetaPtr(std::shared_ptr<FrameMeta> meta) { this->m_metaPtr = meta; }
    uint64_t getCurrentPts1() const override { return this->m_currentPts1; }
    uint64_t getCurrentPts2() const override { return this->m_currentPts2; }
    void setCurrentPts1(uint64_t val) { m_currentPts1 = val; }
    void setCurrentPts2(uint64_t val) { m_currentPts2 = val; }
};

class DummyFrameMeta : public FrameMeta {
  public:
    DummyFrameMeta() {
        setYWidth(1920);
        setYHeight(1080);
        setUVWidth(960);
        setUVHeight(540);
        setPixelFormat(AV_PIX_FMT_YUV420P);
        setTimeBase({1, 25});
        setDuration(10000);
        setTotalFrames(10);
        setColorSpace(AVCOL_SPC_BT709);
        setColorRange(AVCOL_RANGE_JPEG);
        setCodecName("h264");
        setFilename("/tmp/test.mp4");
    }
};

class TestDiffWindow : public QObject {
    Q_OBJECT
  private slots:
    void testConstructionAndAspectRatio();
    void testSettersAndGetters();
    void testAllSettersAndSignals();
    void testFrameAndSelectionMethods();
    void testGettersAndPaintNode();
    void testGetDiffValue();
    void testZoomAt();
    void testZoomToSelection();
    void testUpdateFrameInfo();
    void testZoomToSelectionGetVideoRect();
    void testConvertToVideoCoordinatesViaZoomAt();
};

void TestDiffWindow::testConvertToVideoCoordinatesViaZoomAt() {
    DiffWindow window;
    SharedViewProperties shared;
    window.setSharedView(&shared);
    window.setWidth(800);
    window.setHeight(400);
    window.setAspectRatio(2, 1); // videoAspect = 2.0

    // The zoomAt method calls convertToVideoCoordinates internally
    // We can verify the effect by checking the sharedView's zoom/center after zoomAt
    qreal initialZoom = shared.zoom();
    QPointF initialCenter(shared.centerX(), shared.centerY());

    window.zoomAt(2.0, QPointF(400, 200)); // Center of window

    // After zoom, zoom should change, and center should be near 0.5, 0.5 (center of video coordinates)
    QVERIFY(shared.zoom() != initialZoom);
    QVERIFY(qAbs(shared.centerX() - 0.5) < 0.01);
    QVERIFY(qAbs(shared.centerY() - 0.5) < 0.01);
}

void TestDiffWindow::testZoomToSelectionGetVideoRect() {
    DiffWindow window;
    window.setWidth(900);
    window.setHeight(400);
    window.setAspectRatio(2, 1); // videoAspect = 2.0

    QRectF selection(100, 50, 200, 100);
    window.setSelectionRect(selection);

    // Before zoom, selection should be set
    QVERIFY(window.hasSelection());
    QCOMPARE(window.getSelectionRect(), selection);

    // Call zoomToSelection, which uses getVideoRect internally
    SharedViewProperties shared;
    window.setSharedView(&shared);
    window.zoomToSelection(selection);

    // After zoom, selection may be cleared or updated
    QVERIFY(!window.isSelecting());
}

void TestDiffWindow::testZoomAt() {
    DiffWindow window;
    window.setWidth(640);
    window.setHeight(480);
    window.zoomAt(2.0, QPointF(100, 100));
    // Should not crash, and isSelecting should be false after zoom
    QVERIFY(!window.isSelecting());
    window.zoomAt(1.0, QPointF(0, 0));
    QVERIFY(!window.isSelecting());
}

void TestDiffWindow::testZoomToSelection() {
    DiffWindow window;
    QRectF rect(10, 10, 100, 100);
    window.setSelectionRect(rect);
    QVERIFY(window.hasSelection());
    QCOMPARE(window.getSelectionRect(), rect);
    window.zoomToSelection(rect);
    // After zoom, selection may be cleared or updated
    QVERIFY(!window.isSelecting());
}

void TestDiffWindow::testUpdateFrameInfo() {
    DiffWindow window;
    QSignalSpy spy1(&window, SIGNAL(currentFrameChanged()));
    QSignalSpy spy2(&window, SIGNAL(currentTimeMsChanged()));
    window.updateFrameInfo(5, 123.4);
    QCOMPARE(window.currentFrame(), 5);
    QCOMPARE(window.currentTimeMs(), 123.4);
    QVERIFY(spy1.count() > 0);
    QVERIFY(spy2.count() > 0);
}

void TestDiffWindow::testGetDiffValue() {

    auto meta1 = std::make_shared<DummyFrameMeta>();
    auto meta2 = std::make_shared<DummyFrameMeta>();
    auto fq1 = std::make_shared<FrameQueue>(meta1);
    auto fq2 = std::make_shared<FrameQueue>(meta2);

    FrameData* frame1 = fq1->getTailFrame(0);
    frame1->setPts(0);
    uint8_t* y1 = frame1->yPtr();
    y1[0] = 100; // pixel (0,0)

    FrameData* frame2 = fq2->getTailFrame(0);
    frame2->setPts(0);
    uint8_t* y2 = frame2->yPtr();
    y2[0] = 90; // pixel (0,0)

    DiffWindow window;
    StubDiffRenderer* renderer = new StubDiffRenderer(&window, meta1);
    window.m_renderer = renderer;
    window.initialize(meta1, fq1, fq2);

    renderer->setCurrentPts1(frame1->pts());
    renderer->setCurrentPts2(frame2->pts());

    QVariant result = window.getDiffValue(0, 0);
    QVERIFY(result.isValid());
    QVERIFY(result.canConvert<QVariantList>());
    QVariantList vals = result.toList();
    QCOMPARE(vals.size(), 3);
    QCOMPARE(vals[0].toInt(), 100); // y1Val
    QCOMPARE(vals[1].toInt(), 90);  // y2Val
    QCOMPARE(vals[2].toInt(), 10);  // diff
}

void TestDiffWindow::testAllSettersAndSignals() {
    DiffWindow window;
    QSignalSpy spy1(&window, SIGNAL(osdStateChanged()));
    QSignalSpy spy2(&window, SIGNAL(displayModeChanged()));
    QSignalSpy spy3(&window, SIGNAL(diffMultiplierChanged()));
    QSignalSpy spy4(&window, SIGNAL(diffMethodChanged()));
    QSignalSpy spy5(&window, SIGNAL(maxZoomChanged()));
    QSignalSpy spy6(&window, SIGNAL(sharedViewChanged()));

    window.setOsdState(1);
    window.toggleOsd();
    window.setDisplayMode(2);
    window.setDiffMultiplier(1.5f);
    window.setDiffMethod(1);
    window.setMaxZoom(3.0);

    SharedViewProperties shared;
    window.setSharedView(&shared);

    QVERIFY(spy1.count() > 0);
    QVERIFY(spy2.count() > 0);
    QVERIFY(spy3.count() > 0);
    QVERIFY(spy4.count() > 0);
    QVERIFY(spy5.count() > 0);
    QVERIFY(spy6.count() > 0);
}

void TestDiffWindow::testFrameAndSelectionMethods() {
    DiffWindow window;
    FrameMeta meta = static_cast<FrameMeta>(DummyFrameMeta());
    // Setup valid meta and buffer for FrameData
    auto metaPtr = std::make_shared<FrameMeta>(meta);
    auto queue1 = std::make_shared<FrameQueue>(metaPtr);
    auto queue2 = std::make_shared<FrameQueue>(metaPtr);
    window.initialize(metaPtr, queue1, queue2);

    int ySize = metaPtr->yWidth() * metaPtr->yHeight();
    int uvSize = metaPtr->uvWidth() * metaPtr->uvHeight();
    auto buffer = std::make_shared<std::vector<uint8_t>>(ySize + 2 * uvSize, 128);
    FrameData* frame1 = new FrameData(ySize, uvSize, buffer, 0);
    FrameData* frame2 = new FrameData(ySize, uvSize, buffer, ySize + uvSize);
    frame1->setPts(42);
    frame2->setPts(42);

    window.uploadFrame(frame1, frame2);
    // Assert that currentFrame is updated (should be >= 0)
    QVERIFY(window.currentFrame() >= 0);

    window.renderFrame();
    // Assert that update() can be called and does not crash
    window.update();

    window.batchIsFull();
    window.batchIsEmpty();
    window.rendererError();

    QRectF rect(10, 10, 100, 100);
    window.setSelectionRect(rect);
    QSignalSpy spy(&window, SIGNAL(zoomChanged()));
    window.clearSelection();
    QVERIFY(spy.count() == 1);
    QCOMPARE(window.getSelectionRect(), QRectF());
    window.resetView();
    window.zoomToSelection(rect);
    window.pan(QPointF(5, 5));

    delete frame1;
    delete frame2;
}

void TestDiffWindow::testGettersAndPaintNode() {
    DiffWindow window;
    FrameMeta meta = static_cast<FrameMeta>(DummyFrameMeta());
    auto metaPtr = std::make_shared<FrameMeta>(meta);

    window.initialize(metaPtr, nullptr, nullptr);
    window.setAspectRatio(1920, 1080);
    QCOMPARE(window.getAspectRatio(), 1920.0 / 1080.0);

    // Assert getters return expected values
    QCOMPARE(window.pixelFormat(), QString("yuv420p"));
    QCOMPARE(window.timeBase(), QString("1/25"));
    QCOMPARE(window.duration(), 10000);
    QVERIFY(window.currentTimeMs() >= 0);
    QVERIFY(window.currentFrame() >= 0);
    QCOMPARE(window.totalFrames(), 10);

    // Assert update() can be called and does not crash
    window.update();
}

void TestDiffWindow::testConstructionAndAspectRatio() {
    DiffWindow window;
    // Use the actual default value set by DiffWindow
    QCOMPARE(window.getAspectRatio(), qreal(16.0 / 9.0));

    window.setAspectRatio(1920, 1080);
    QCOMPARE(window.getAspectRatio(), qreal(1920.0 / 1080.0));

    window.setAspectRatio(0, 0);
    QCOMPARE(window.getAspectRatio(), qreal(1920.0 / 1080.0));
    // Should not crash, aspect ratio remains unchanged
}

void TestDiffWindow::testSettersAndGetters() {
    DiffWindow window;
    SharedViewProperties sharedView;
    window.setSharedView(&sharedView);
    QCOMPARE(window.sharedView(), &sharedView);

    window.setMaxZoom(2.0);
    QCOMPARE(window.maxZoom(), 2.0);

    window.setOsdState(1);
    window.toggleOsd();
    QCOMPARE(window.currentFrame(), 0);

    // Create FrameMeta, FrameQueue, SharedViewProperties directly
    FrameMeta meta = static_cast<FrameMeta>(DummyFrameMeta());
    // Setup valid meta and buffer for FrameData
    auto metaPtr = std::make_shared<FrameMeta>(meta);

    auto queue1 = std::make_shared<FrameQueue>(metaPtr);
    auto queue2 = std::make_shared<FrameQueue>(metaPtr);
    window.initialize(metaPtr, queue1, queue2);

    QCOMPARE(window.totalFrames(), 10);
    QCOMPARE(window.pixelFormat(), QString("yuv420p"));
    QCOMPARE(window.timeBase(), QString("1/25"));
    QCOMPARE(window.duration(), 10000);
}

QTEST_MAIN(TestDiffWindow)
#include "test_diffwindow.moc"

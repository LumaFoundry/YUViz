#include <QtTest>
#include <memory>
#include "frames/frameMeta.h"
#include "ui/videoWindow.h"
#include "utils/sharedViewProperties.h"

// Stub VideoRenderer for safe unit testing
class StubVideoRenderer : public VideoRenderer {
  public:
    StubVideoRenderer(QObject* parent = nullptr) :
        VideoRenderer(parent, nullptr) {}
    void uploadFrame(FrameData* frame) override {}
    void releaseBatch() override {}
    void setMetaPtr(std::shared_ptr<FrameMeta> meta) { this->m_metaPtr = meta; }
    void setCurrentFrame(FrameData* frame) { this->m_currentFrame = frame; }
    FrameData* getCurrentFrame() const override { return this->m_currentFrame; }

    void setColorParams(AVColorSpace, AVColorRange) override {}
};

class DummyFrameMeta : public FrameMeta {
  public:
    DummyFrameMeta() {
        setYWidth(640);
        setYHeight(480);
        setUVWidth(320);
        setUVHeight(240);
        setPixelFormat(AV_PIX_FMT_YUV420P);
        setTimeBase({1, 1000});
        setDuration(10000);
        setColorSpace(AVCOL_SPC_BT709);
        setColorRange(AVCOL_RANGE_JPEG);
        setCodecName("h264");
        setFilename("/tmp/test.mp4");
    }
};

class TestVideoWindow : public QObject {
    Q_OBJECT
  private slots:
    void testAspectRatioProperty();
    void testMaxZoomProperty();
    void testOsdStateProperty();
    void testComponentDisplayModeProperty();
    void testSignalEmissions();
    void testSetSharedView();
    void testUploadFrameSignal();
    void testToggleOsd();
    void testUpdateFrameInfo();
    void testClearSelection();
    void testSetSelectionRect();
    void testPan();
    void testResetView();
    void testZoomAt();
    void testZoomToSelection();
    void testGetYUV();
    void testGetFrameMeta();
    void testInfoProperties();
    void testSyncColorSpaceMenu();

    void testSetColorParams();
    void testBatchIsFull();
    void testBatchIsEmpty();
    void testRendererError();

    void testRenderFrameUpdatesInfo();
    void testZoomToSelectionCoversGetVideoRect();
};

void TestVideoWindow::testAspectRatioProperty() {
    VideoWindow vw;
    vw.setAspectRatio(1920, 1080);
    QCOMPARE(vw.getAspectRatio(), qreal(1920.0 / 1080.0));
}

void TestVideoWindow::testMaxZoomProperty() {
    VideoWindow vw;
    QSignalSpy spy(&vw, SIGNAL(maxZoomChanged()));
    vw.setMaxZoom(5000.0);
    QCOMPARE(vw.maxZoom(), 5000.0);
    QVERIFY(spy.count() == 1);
}

void TestVideoWindow::testOsdStateProperty() {
    VideoWindow vw;
    vw.setOsdState(2);
    QCOMPARE(vw.osdState(), 2);
    vw.setOsdState(1);
    QCOMPARE(vw.osdState(), 1);
}

void TestVideoWindow::testComponentDisplayModeProperty() {
    VideoWindow vw;
    vw.setComponentDisplayMode(2);
    QCOMPARE(vw.componentDisplayMode(), 2);
    vw.setComponentDisplayMode(0);
    QCOMPARE(vw.componentDisplayMode(), 0);
}

void TestVideoWindow::testSignalEmissions() {
    VideoWindow vw;
    QSignalSpy spy1(&vw, SIGNAL(osdStateChanged(int)));
    QSignalSpy spy2(&vw, SIGNAL(componentDisplayModeChanged()));
    vw.setOsdState(1);
    vw.setComponentDisplayMode(2);
    QVERIFY(spy1.count() > 0);
    QVERIFY(spy2.count() > 0);
}

void TestVideoWindow::testSetSharedView() {
    VideoWindow vw;
    SharedViewProperties shared;
    QSignalSpy spy(&vw, SIGNAL(sharedViewChanged()));
    vw.setSharedView(&shared);
    QCOMPARE(vw.sharedView(), &shared);
    QVERIFY(spy.count() == 1);
}

void TestVideoWindow::testUploadFrameSignal() {
    VideoWindow vw;
    QSignalSpy spy(&vw, SIGNAL(frameReady()));
    vw.m_renderer = new StubVideoRenderer(); // Use stub to avoid segfault
    vw.uploadFrame(nullptr);                 // Should emit frameReady
    QVERIFY(spy.count() == 1);
}

void TestVideoWindow::testToggleOsd() {
    VideoWindow vw;
    int initial = vw.osdState();
    vw.toggleOsd();
    QCOMPARE(vw.osdState(), (initial + 1) % 3);
    vw.toggleOsd();
    QCOMPARE(vw.osdState(), (initial + 2) % 3);
}

void TestVideoWindow::testUpdateFrameInfo() {
    VideoWindow vw;
    QSignalSpy spy1(&vw, SIGNAL(currentFrameChanged()));
    QSignalSpy spy2(&vw, SIGNAL(currentTimeMsChanged()));
    vw.updateFrameInfo(5, 123.4);
    QCOMPARE(vw.currentFrame(), 5);
    QCOMPARE(vw.currentTimeMs(), 123.4);
    QVERIFY(spy1.count() > 0);
    QVERIFY(spy2.count() > 0);
}

void TestVideoWindow::testClearSelection() {
    VideoWindow vw;
    QRectF rect(10, 10, 100, 100);
    vw.setSelectionRect(rect);
    QSignalSpy spy(&vw, SIGNAL(zoomChanged()));
    vw.clearSelection();
    QVERIFY(spy.count() == 1);
    QVERIFY(!vw.hasSelection());
    QCOMPARE(vw.getSelectionRect(), QRectF());
}

void TestVideoWindow::testSetSelectionRect() {
    VideoWindow vw;
    QRectF rect(10, 10, 100, 100);
    vw.setSelectionRect(rect);
    QVERIFY(vw.hasSelection());
    QCOMPARE(vw.getSelectionRect(), rect);
    vw.clearSelection();
    QVERIFY(!vw.hasSelection());
}

void TestVideoWindow::testPan() {
    VideoWindow vw;

    SharedViewProperties shared;
    vw.setSharedView(&shared);
    vw.setWidth(640);
    vw.setHeight(480);
    vw.pan(QPointF(10, 10));
}

void TestVideoWindow::testResetView() {
    VideoWindow vw;
    SharedViewProperties shared;
    vw.setSharedView(&shared);
    vw.resetView();
}

void TestVideoWindow::testZoomAt() {
    VideoWindow vw;
    SharedViewProperties shared;
    vw.setSharedView(&shared);
    vw.setWidth(640);
    vw.setHeight(480);
    vw.zoomAt(2.0, QPointF(100, 100));
}

void TestVideoWindow::testZoomToSelection() {
    VideoWindow vw;
    SharedViewProperties shared;
    vw.setSharedView(&shared);
    QRectF rect(10, 10, 100, 100);
    vw.zoomToSelection(rect);
}

void TestVideoWindow::testGetYUV() {
    VideoWindow vw;
    vw.m_renderer = new StubVideoRenderer();
    DummyFrameMeta* meta = new DummyFrameMeta();
    int ySize = 640 * 480;
    int uvSize = 320 * 240;
    auto buffer = std::make_shared<std::vector<uint8_t>>(ySize + 2 * uvSize, 128);
    FrameData* frame = new FrameData(ySize, uvSize, buffer, 0);
    frame->setPts(42);
    auto stubRenderer = static_cast<StubVideoRenderer*>(vw.m_renderer);
    stubRenderer->setMetaPtr(std::shared_ptr<FrameMeta>(meta));
    stubRenderer->setCurrentFrame(frame);
    QVariant result = vw.getYUV(0, 0);
    QVERIFY(result.isValid());
    QVERIFY(result.canConvert<QVariantList>());
}

void TestVideoWindow::testGetFrameMeta() {
    VideoWindow vw;
    vw.m_renderer = new StubVideoRenderer();
    DummyFrameMeta* meta = new DummyFrameMeta();
    auto stubRenderer = static_cast<StubVideoRenderer*>(vw.m_renderer);
    stubRenderer->setMetaPtr(std::shared_ptr<FrameMeta>(meta));
    QVariantMap result = vw.getFrameMeta();
    QVERIFY(result.contains("yWidth"));
    QVERIFY(result.contains("format"));
}

void TestVideoWindow::testInfoProperties() {
    VideoWindow vw;
    DummyFrameMeta* meta = new DummyFrameMeta();
    vw.initialize(std::shared_ptr<FrameMeta>(meta));

    QCOMPARE(vw.colorSpace(), QString("BT.709"));
    QCOMPARE(vw.colorRange(), QString("Full"));
    QCOMPARE(vw.videoResolution(), QString("640x480"));
    QCOMPARE(vw.codecName(), QString("h264"));
    QCOMPARE(vw.videoName(), QString("test.mp4"));
    QVERIFY(!vw.pixelFormat().isEmpty());
    QVERIFY(!vw.timeBase().isEmpty());
    QVERIFY(vw.duration() == 10000);

    // Test additional color spaces and pixel formats
    DummyFrameMeta* meta2 = new DummyFrameMeta();
    meta2->setColorSpace(AVCOL_SPC_BT470BG);
    meta2->setColorRange(AVCOL_RANGE_MPEG);
    meta2->setPixelFormat(AV_PIX_FMT_YUYV422);
    vw.initialize(std::shared_ptr<FrameMeta>(meta2));
    QCOMPARE(vw.colorSpace(), QString("BT.470BG"));
    QCOMPARE(vw.colorRange(), QString("Limited"));
    QVERIFY(!vw.pixelFormat().isEmpty());

    // Additional color space/range combos
    struct ColorSpaceTest {
        AVColorSpace colorSpace;
        AVColorRange colorRange;
        QString expectedSpace;
        QString expectedRange;
    };

    ColorSpaceTest combos[] = {{AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, "BT.2020 NCL", "Limited"},
                               {AVCOL_SPC_BT2020_CL, AVCOL_RANGE_MPEG, "BT.2020 CL", "Limited"},
                               {AVCOL_SPC_SMPTE2085, AVCOL_RANGE_MPEG, "SMPTE 2085", "Limited"},
                               {AVCOL_SPC_CHROMA_DERIVED_NCL, AVCOL_RANGE_MPEG, "Chroma Derived NCL", "Limited"},
                               {AVCOL_SPC_CHROMA_DERIVED_CL, AVCOL_RANGE_MPEG, "Chroma Derived CL", "Limited"},
                               {AVCOL_SPC_ICTCP, AVCOL_RANGE_MPEG, "ICtCp", "Limited"},
                               {AVCOL_SPC_SMPTE240M, AVCOL_RANGE_MPEG, "SMPTE 240M", "Limited"},
                               {AVCOL_SPC_SMPTE170M, AVCOL_RANGE_MPEG, "SMPTE 170M", "Limited"},
                               {AVCOL_SPC_UNSPECIFIED, AVCOL_RANGE_UNSPECIFIED, "Unspecified", "Unspecified"}};

    for (const auto& combo : combos) {
        DummyFrameMeta* meta = new DummyFrameMeta();
        meta->setColorSpace(combo.colorSpace);
        meta->setColorRange(combo.colorRange);
        vw.initialize(std::shared_ptr<FrameMeta>(meta));
        QCOMPARE(vw.colorSpace(), combo.expectedSpace);
        QCOMPARE(vw.colorRange(), combo.expectedRange);
        QVERIFY(!vw.pixelFormat().isEmpty());
    }

    DummyFrameMeta* meta3 = new DummyFrameMeta();
    meta3->setColorSpace(AVCOL_SPC_RGB);
    meta3->setColorRange(AVCOL_RANGE_UNSPECIFIED);
    meta3->setPixelFormat(AV_PIX_FMT_UYVY422);
    vw.initialize(std::shared_ptr<FrameMeta>(meta3));
    QCOMPARE(vw.colorSpace(), QString("RGB"));
    QCOMPARE(vw.colorRange(), QString("Unspecified"));
    QVERIFY(!vw.pixelFormat().isEmpty());

    // Try different pixel formats
    auto stubRenderer = static_cast<StubVideoRenderer*>(vw.m_renderer);
    // YUYV422: 2 bytes per pixel (packed)
    DummyFrameMeta* metaYUYV = new DummyFrameMeta();
    metaYUYV->setPixelFormat(AV_PIX_FMT_YUYV422);
    int yWidthYUYV = metaYUYV->yWidth();
    int yHeightYUYV = metaYUYV->yHeight();
    int packedSizeYUYV = yWidthYUYV * yHeightYUYV * 2;
    auto bufferYUYV = std::make_shared<std::vector<uint8_t>>(packedSizeYUYV, 128);
    FrameData* frameYUYV = new FrameData(packedSizeYUYV, 0, bufferYUYV, 0);
    frameYUYV->setPts(42);
    stubRenderer->setMetaPtr(std::shared_ptr<FrameMeta>(metaYUYV));
    stubRenderer->setCurrentFrame(frameYUYV);
    QVariant resultYUYV = vw.getYUV(0, 0);
    QVERIFY(resultYUYV.isValid());

    // UYVY422: 2 bytes per pixel (packed)
    DummyFrameMeta* metaUYVY = new DummyFrameMeta();
    metaUYVY->setPixelFormat(AV_PIX_FMT_UYVY422);
    int yWidthUYVY = metaUYVY->yWidth();
    int yHeightUYVY = metaUYVY->yHeight();
    int packedSizeUYVY = yWidthUYVY * yHeightUYVY * 2;
    auto bufferUYVY = std::make_shared<std::vector<uint8_t>>(packedSizeUYVY, 128);
    FrameData* frameUYVY = new FrameData(packedSizeUYVY, 0, bufferUYVY, 0);
    frameUYVY->setPts(42);
    stubRenderer->setMetaPtr(std::shared_ptr<FrameMeta>(metaUYVY));
    stubRenderer->setCurrentFrame(frameUYVY);
    QVariant resultUYVY = vw.getYUV(0, 0);
    QVERIFY(resultUYVY.isValid());

    // YUV422P: Planar 4:2:2
    DummyFrameMeta* metaYUV422P = new DummyFrameMeta();
    metaYUV422P->setPixelFormat(AV_PIX_FMT_YUV422P);
    int yWidthYUV422P = metaYUV422P->yWidth();
    int yHeightYUV422P = metaYUV422P->yHeight();
    int uvWidthYUV422P = metaYUV422P->uvWidth();
    int uvHeightYUV422P = metaYUV422P->uvHeight();
    int ySizeYUV422P = yWidthYUV422P * yHeightYUV422P;
    int uSizeYUV422P = uvWidthYUV422P * uvHeightYUV422P;
    int vSizeYUV422P = uvWidthYUV422P * uvHeightYUV422P;
    auto bufferYUV422P = std::make_shared<std::vector<uint8_t>>(ySizeYUV422P + uSizeYUV422P + vSizeYUV422P, 128);
    FrameData* frameYUV422P = new FrameData(ySizeYUV422P, uSizeYUV422P + vSizeYUV422P, bufferYUV422P, 0);
    frameYUV422P->setPts(42);
    stubRenderer->setMetaPtr(std::shared_ptr<FrameMeta>(metaYUV422P));
    stubRenderer->setCurrentFrame(frameYUV422P);
    QVariant resultYUV422P = vw.getYUV(0, 0);
    QVERIFY(resultYUV422P.isValid());

    // YUV444P: Planar 4:4:4
    DummyFrameMeta* metaYUV444P = new DummyFrameMeta();
    metaYUV444P->setPixelFormat(AV_PIX_FMT_YUV444P);
    int yWidthYUV444P = metaYUV444P->yWidth();
    int yHeightYUV444P = metaYUV444P->yHeight();
    int uvWidthYUV444P = metaYUV444P->uvWidth();
    int uvHeightYUV444P = metaYUV444P->uvHeight();
    int ySizeYUV444P = yWidthYUV444P * yHeightYUV444P;
    int uSizeYUV444P = uvWidthYUV444P * uvHeightYUV444P;
    int vSizeYUV444P = uvWidthYUV444P * uvHeightYUV444P;
    auto bufferYUV444P = std::make_shared<std::vector<uint8_t>>(ySizeYUV444P + uSizeYUV444P + vSizeYUV444P, 128);
    FrameData* frameYUV444P = new FrameData(ySizeYUV444P, uSizeYUV444P + vSizeYUV444P, bufferYUV444P, 0);
    frameYUV444P->setPts(42);
    stubRenderer->setMetaPtr(std::shared_ptr<FrameMeta>(metaYUV444P));
    stubRenderer->setCurrentFrame(frameYUV444P);
    QVariant resultYUV444P = vw.getYUV(0, 0);
    QVERIFY(resultYUV444P.isValid());

    // Default: Unknown format (simulate by using an invalid pixel format)
    DummyFrameMeta* metaUnknown = new DummyFrameMeta();
    metaUnknown->setPixelFormat(static_cast<AVPixelFormat>(-1));
    int yWidthUnknown = metaUnknown->yWidth();
    int yHeightUnknown = metaUnknown->yHeight();
    int uvWidthUnknown = metaUnknown->uvWidth();
    int uvHeightUnknown = metaUnknown->uvHeight();
    int ySizeUnknown = yWidthUnknown * yHeightUnknown;
    int uSizeUnknown = uvWidthUnknown * uvHeightUnknown;
    int vSizeUnknown = uvWidthUnknown * uvHeightUnknown;
    auto bufferUnknown = std::make_shared<std::vector<uint8_t>>(ySizeUnknown + uSizeUnknown + vSizeUnknown, 128);
    FrameData* frameUnknown = new FrameData(ySizeUnknown, uSizeUnknown + vSizeUnknown, bufferUnknown, 0);
    frameUnknown->setPts(42);
    stubRenderer->setMetaPtr(std::shared_ptr<FrameMeta>(metaUnknown));
    stubRenderer->setCurrentFrame(frameUnknown);
    QVariant resultUnknown = vw.getYUV(0, 0);
    QVERIFY(resultUnknown.isValid());

    // NV12: Y plane + UV interleaved plane
    DummyFrameMeta* metaNV12 = new DummyFrameMeta();
    metaNV12->setPixelFormat(AV_PIX_FMT_NV12);
    int yWidthNV12 = metaNV12->yWidth();
    int yHeightNV12 = metaNV12->yHeight();
    int uvWidthNV12 = metaNV12->uvWidth();
    int uvHeightNV12 = metaNV12->uvHeight();
    int ySizeNV12 = yWidthNV12 * yHeightNV12;
    int uvSizeNV12 = uvWidthNV12 * uvHeightNV12 * 2; // Interleaved UV
    auto bufferNV12 = std::make_shared<std::vector<uint8_t>>(ySizeNV12 + uvSizeNV12, 128);
    FrameData* frameNV12 = new FrameData(ySizeNV12, uvSizeNV12, bufferNV12, 0);
    frameNV12->setPts(42);
    stubRenderer->setMetaPtr(std::shared_ptr<FrameMeta>(metaNV12));
    stubRenderer->setCurrentFrame(frameNV12);
    QVariant resultNV12 = vw.getYUV(0, 0);
    QVERIFY(resultNV12.isValid());

    // NV21: Y plane + VU interleaved plane
    DummyFrameMeta* metaNV21 = new DummyFrameMeta();
    metaNV21->setPixelFormat(AV_PIX_FMT_NV21);
    int yWidthNV21 = metaNV21->yWidth();
    int yHeightNV21 = metaNV21->yHeight();
    int uvWidthNV21 = metaNV21->uvWidth();
    int uvHeightNV21 = metaNV21->uvHeight();
    int ySizeNV21 = yWidthNV21 * yHeightNV21;
    int uvSizeNV21 = uvWidthNV21 * uvHeightNV21 * 2; // Interleaved VU
    auto bufferNV21 = std::make_shared<std::vector<uint8_t>>(ySizeNV21 + uvSizeNV21, 128);
    FrameData* frameNV21 = new FrameData(ySizeNV21, uvSizeNV21, bufferNV21, 0);
    frameNV21->setPts(42);
    stubRenderer->setMetaPtr(std::shared_ptr<FrameMeta>(metaNV21));
    stubRenderer->setCurrentFrame(frameNV21);
    QVariant resultNV21 = vw.getYUV(0, 0);
    QVERIFY(resultNV21.isValid());
}

void TestVideoWindow::testSyncColorSpaceMenu() {
    VideoWindow vw;
    vw.m_renderer = new StubVideoRenderer();
    DummyFrameMeta* meta = new DummyFrameMeta();
    auto stubRenderer = static_cast<StubVideoRenderer*>(vw.m_renderer);
    stubRenderer->setMetaPtr(std::shared_ptr<FrameMeta>(meta));
    vw.syncColorSpaceMenu(); // Should not crash
}

void TestVideoWindow::testSetColorParams() {
    VideoWindow vw;
    StubVideoRenderer* stub = new StubVideoRenderer();
    vw.m_renderer = stub;
    // We'll just call the method to ensure it doesn't crash; actual renderer logic can be checked in integration
    vw.setColorParams(AVCOL_SPC_BT709, AVCOL_RANGE_JPEG);
}

void TestVideoWindow::testBatchIsFull() {
    VideoWindow vw;
    QSignalSpy spy(&vw, SIGNAL(batchUploaded(bool)));
    vw.batchIsFull();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
}

void TestVideoWindow::testBatchIsEmpty() {
    VideoWindow vw;
    QSignalSpy spy(&vw, SIGNAL(gpuUploaded(bool)));
    vw.batchIsEmpty();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
}

void TestVideoWindow::testRendererError() {
    VideoWindow vw;
    QSignalSpy spy(&vw, SIGNAL(errorOccurred()));
    vw.rendererError();
    QCOMPARE(spy.count(), 1);
}

void TestVideoWindow::testRenderFrameUpdatesInfo() {
    VideoWindow vw;
    DummyFrameMeta* meta = new DummyFrameMeta();
    meta->setTimeBase({1, 1000}); // 1 ms per unit
    vw.initialize(std::shared_ptr<FrameMeta>(meta));
    auto stubRenderer = new StubVideoRenderer();
    vw.m_renderer = stubRenderer; // Overwrite with stub after initialize

    // Create a frame with a known pts
    int ySize = meta->yWidth() * meta->yHeight();
    int uvSize = meta->uvWidth() * meta->uvHeight() * 2;
    auto buffer = std::make_shared<std::vector<uint8_t>>(ySize + uvSize, 128);
    FrameData* frame = new FrameData(ySize, uvSize, buffer, 0);
    frame->setPts(123); // 123 units
    stubRenderer->setCurrentFrame(frame);

    // Connect to currentFrameChanged and currentTimeMsChanged
    QSignalSpy frameSpy(&vw, SIGNAL(currentFrameChanged()));
    QSignalSpy timeSpy(&vw, SIGNAL(currentTimeMsChanged()));

    vw.renderFrame();

    // Should have updated frame info
    QCOMPARE(vw.currentFrame(), 123);
    QCOMPARE(vw.currentTimeMs(), 123.0); // 123 * 1ms
    QVERIFY(frameSpy.count() > 0);
    QVERIFY(timeSpy.count() > 0);
}

void TestVideoWindow::testZoomToSelectionCoversGetVideoRect() {
    VideoWindow vw;
    SharedViewProperties shared;
    vw.setSharedView(&shared);
    vw.setWidth(900);
    vw.setHeight(400);
    vw.setAspectRatio(2, 1); // videoAspect = 2.0

    QRectF selection(100, 50, 200, 100);
    vw.setSelectionRect(selection);

    // Before zoom, selection should be set
    QVERIFY(vw.hasSelection());
    QCOMPARE(vw.getSelectionRect(), selection);

    // Call zoomToSelection, which uses getVideoRect internally
    vw.zoomToSelection(selection);

    // After zoom, selection may be cleared or shared view may be updated
    // We check that the shared view's zoom or center has changed from default
    QVERIFY(shared.zoom() != 1.0 || shared.centerX() != 0.5 || shared.centerY() != 0.5);
}

QTEST_MAIN(TestVideoWindow)
#include "test_videowindow.moc"

#include <QtTest>
#include "controller/frameController.h"
#include "decoder/videoDecoder.h"
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "frames/frameQueue.h"
#include "rendering/videoRenderer.h"
#include "utils/appConfig.h"
#include "utils/errorReporter.h"
#include "utils/videoFileInfo.h"

class StubDecoder : public VideoDecoder {
    Q_OBJECT
  public:
    void setFileName(const std::string&) {}
    void setDimensions(int, int) {}
    void setFramerate(double) {}
    void setFormat(int) {}
    void setForceSoftwareDecoding(bool) {}
    void openFile() override {}
    void setFrameQueue(std::shared_ptr<FrameQueue> queue) { m_frameQueue = queue; }
    FrameMeta getMetaData() override {
        FrameMeta meta;
        // Set reasonable values
        meta.setTotalFrames(4); // Increase to allow valid missing PTS
        meta.setDuration(4);
        meta.setYWidth(1);
        meta.setYHeight(1);
        meta.setUVWidth(2);
        meta.setUVHeight(2);
        meta.setTimeBase({1, 1}); // Ensure time base matches test expectation
        return meta;
    }
    void loadFrames(int, int) { emit framesLoaded(true); }
    void seek(int64_t, int) { emit frameSeeked(0); }
  signals:
    void framesLoaded(bool);
    void frameSeeked(int64_t);

  private:
    std::shared_ptr<FrameQueue> m_frameQueue;
};

class StubRenderer : public VideoRenderer {
    Q_OBJECT
  public:
    StubRenderer(QObject* parent = nullptr, std::shared_ptr<FrameMeta> metaPtr = nullptr) :
        VideoRenderer(parent, metaPtr ? metaPtr : std::make_shared<FrameMeta>()) {}
    bool batchFullEmitted = false;
    bool batchEmptyEmitted = false;
    bool rendererErrorEmitted = false;

  signals:
    void batchIsFull();
    void batchIsEmpty();
    void rendererError();
};

class StubVideoWindow : public VideoWindow {
    Q_OBJECT
  public:
    StubRenderer* stubRenderer;
    bool initialized = false;
    bool syncColorSpaceMenuCalled = false;
    StubVideoWindow(QQuickItem* parent = nullptr) :
        VideoWindow(parent) {
        stubRenderer = new StubRenderer(parent, std::make_shared<FrameMeta>());
        m_renderer = stubRenderer;
    }
    ~StubVideoWindow() override { delete stubRenderer; }
    void initialize(std::shared_ptr<FrameMeta> meta) { initialized = true; }
    void uploadFrame(FrameData*, int) {}
    void renderFrame(int) {}
    void syncColorSpaceMenu() { syncColorSpaceMenuCalled = true; }
};

VideoFileInfo makeStubVideoFileInfo(StubVideoWindow*& windowOut) {
    VideoFileInfo info;
    info.filename = "dummy";
    info.width = 2; // Use >1 for valid frame sizes
    info.height = 2;
    info.framerate = 30.0;
    info.pixelFormat = AV_PIX_FMT_YUV420P;
    info.forceSoftwareDecoding = false;
    windowOut = new StubVideoWindow();
    info.windowPtr = windowOut;
    return info;
}

class FrameControllerTest : public QObject {
    Q_OBJECT
  private slots:
    void testConstructorAndStart();
    void testOnTimerTickAndStep();
    void testOnFrameDecoded();
    void testOnFrameUploaded();
    void testOnFrameRendered();
    void testOnSeekAndFrameSeeked();
    void testOnRenderError();
    void testClearStall();
    void testGetTimeBaseAndMeta();
    void testErrorPaths();
    void testEdgeCasesAndSignals();
    void testDestructorLogic();
    void testErrorAndNullDecoderBranches();
    void testOnFrameRenderedBranches();
    void testOnSeekBranches();
    void testOnFrameSeekedEndFrame();
};

void FrameControllerTest::testOnFrameSeekedEndFrame() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 99, decoder);
    auto fq = fc.getFrameQueue();
    // Set up a valid frame at PTS 2 and mark as end frame
    FrameData* frame = fq->getTailFrame(2);
    frame->setPts(2);
    frame->setEndFrame(true);
    // Call onFrameSeeked with PTS 2
    QSignalSpy uploadSpy(&fc, SIGNAL(requestUpload(FrameData*, int)));
    QSignalSpy endSpy(&fc, SIGNAL(endOfVideo(bool, int)));
    fc.onFrameSeeked(2);
    QCoreApplication::processEvents();
    // Should emit requestUpload and endOfVideo (with m_endOfVideo true)
    QVERIFY(uploadSpy.count() > 0);
    QVERIFY(endSpy.count() > 0);
    // Optionally check that m_endOfVideo is true
    delete window;
}

void FrameControllerTest::testOnFrameRenderedBranches() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 15, decoder);
    QSignalSpy startSpy(&fc, SIGNAL(startOfVideo(int)));
    auto fq = fc.getFrameQueue();
    // Case 1: futurePts < 0
    // Ensure frame queue is empty for PTS 0
    fq->getTailFrame(0)->setPts(0); // Make sure getHeadFrame(0) returns nullptr
    fc.start();
    fc.onTimerTick(0, -1); // sets m_ticking = 0, m_direction = -1
    fc.onFrameRendered();  // Should emit startOfVideo
    QCoreApplication::processEvents();
    QVERIFY(startSpy.count() > 0);

    // Case 2: future frame present and not stale
    fq->getTailFrame(1)->setPts(1);
    fc.onTimerTick(1, 1); // sets m_ticking = 1, m_direction = 1
    // Ensure frame for futurePts = 2 exists and is not stale
    FrameData* futureFrame = fq->getTailFrame(2);
    futureFrame->setPts(2); // positive PTS, not stale
    QSignalSpy uploadSpy(&fc, SIGNAL(requestUpload(FrameData*, int)));
    fc.onFrameRendered(); // Should emit requestUpload
    QCoreApplication::processEvents();
    QVERIFY(uploadSpy.count() > 0);

    // Case 3: not end of video / start of video, decode requested
    fq->getTailFrame(2)->setPts(2);
    fc.onTimerTick(2, 1);
    fq->getTailFrame(3)->setPts(3);
    QSignalSpy decodeSpy(&fc, SIGNAL(requestDecode(int, int)));
    fc.onFrameDecoded(true);
    fc.onFrameRendered(); // Should emit requestDecode
    QCoreApplication::processEvents();
    QVERIFY(decodeSpy.count() > 0);
    delete window;
}

void FrameControllerTest::testOnSeekBranches() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 16, decoder);
    auto fq = fc.getFrameQueue();
    // Case 1: frame present and not stale
    FrameData* frame1 = fq->getTailFrame(1);
    frame1->setPts(1); // positive PTS, not stale
    fq->updateTail(1); // Ensure tail pointer is up to date so isStale(1) returns false
    QSignalSpy uploadSpy(&fc, SIGNAL(requestUpload(FrameData*, int)));
    QSignalSpy decodeSpy(&fc, SIGNAL(requestDecode(int, int)));
    fc.onSeek(1);
    QCoreApplication::processEvents();
    QVERIFY(uploadSpy.count() > 0);
    QVERIFY(decodeSpy.count() > 0);

    // Case 2: frame present and stale
    FrameData* frame2 = fq->getTailFrame(2);
    frame2->setPts(-2); // negative PTS, simulate stale
    QSignalSpy seekSpy(&fc, SIGNAL(requestSeek(int64_t, int)));
    fc.onSeek(2);
    QCoreApplication::processEvents();
    QVERIFY(seekSpy.count() > 0);

    // Case 3: frame missing
    // Simulate missing by setting PTS to -1 (or not setting at all)
    fq->getTailFrame(3)->setPts(-1);
    QSignalSpy seekSpy2(&fc, SIGNAL(requestSeek(int64_t, int)));
    fc.onSeek(3);
    QCoreApplication::processEvents();
    QVERIFY(seekSpy2.count() > 0);
    delete window;
}

void FrameControllerTest::testErrorAndNullDecoderBranches() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    // Test error path in onFrameDecoded(false)
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 12, decoder);
    fc.onFrameDecoded(false); // Should call ErrorReporter
    // Test error path in onRenderError
    fc.onRenderError(); // Should call ErrorReporter
    // Test onFrameSeeked with nullptr frame
    QSignalSpy endSpy(&fc, SIGNAL(endOfVideo(bool, int)));
    fc.onFrameSeeked(999); // Should emit endOfVideo
    QVERIFY(endSpy.count() > 0);
    // Test onFrameUploaded with nullptr frame for seeking
    fc.onSeek(3);
    fc.onFrameUploaded(); // Should emit endOfVideo for missing frame
    QVERIFY(endSpy.count() > 1);
    // Test stall clearing only via public API
    // Trigger stall
    fc.onTimerTick(1, 1); // Should stall
    QCoreApplication::processEvents();
    // Simulate decoder catching up: add frame to queue and call onFrameDecoded
    auto fq = fc.getFrameQueue();
    fq->getTailFrame(1)->setPts(1);
    fc.onFrameDecoded(true); // Should clear stall
    delete window;
}

void FrameControllerTest::testEdgeCasesAndSignals() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 10, decoder);
    auto fq = fc.getFrameQueue();
    // Test negative PTS for onTimerStep
    QSignalSpy endSpy(&fc, SIGNAL(endOfVideo(bool, int)));
    fc.onTimerStep(-5, 1);
    QVERIFY(endSpy.count() > 0);
    // Test end-of-video logic in onTimerTick
    fq->getTailFrame(0)->setPts(0);
    fq->getTailFrame(0)->setEndFrame(true);
    QSignalSpy renderSpy(&fc, SIGNAL(requestRender(int)));
    fc.onTimerTick(0, 1);
    QVERIFY(renderSpy.count() > 0);
    // Test start-of-video logic in onFrameRendered
    fc.onFrameRendered(); // Should emit startOfVideo if futurePts < 0
    // Test requestSeek with negative PTS in onTimerStep
    fc.onTimerStep(-10, -1);
    // Test seekCompleted signal in onFrameUploaded
    fc.onSeek(2);
    fq->getTailFrame(2)->setPts(2);
    fc.onFrameUploaded();
    // Test ready signal in onFrameUploaded (prefill)
    fc.start();
    fc.onFrameDecoded(true);
    fc.onFrameUploaded();
    delete window;
}

void FrameControllerTest::testDestructorLogic() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController* fc = new FrameController(nullptr, info, 11, decoder);
    // Delete controller to trigger destructor logic (thread quit/wait, pointer reset)
    delete fc;
    delete window;
}

void FrameControllerTest::testConstructorAndStart() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 0, decoder);
    auto fq = fc.getFrameQueue();
    if (fq->getSize() > 0) {
        fq->getTailFrame(0)->setPts(0);
    }
    QSignalSpy spy(&fc, SIGNAL(requestDecode(int, int)));
    fc.start();
    qDebug() << "window->initialized after start:" << window->initialized;
    QCOMPARE(spy.count(), 1);
    // Check frame presence after start
    FrameData* frame = fq->getHeadFrame(0);
    qDebug() << "FrameData for PTS 0 after start:" << (frame != nullptr);
    delete window;
}

void FrameControllerTest::testOnTimerTickAndStep() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 1, decoder);
    auto fq = fc.getFrameQueue();
    if (fq->getSize() > 0) {
        fq->getTailFrame(0)->setPts(0);
    }
    fc.start();
    FrameData* frame = fq->getHeadFrame(0);
    qDebug() << "FrameData for PTS 0 after start:" << (frame != nullptr);
    QSignalSpy renderSpy(&fc, SIGNAL(requestRender(int)));
    QSignalSpy endSpy(&fc, SIGNAL(endOfVideo(bool, int)));
    fc.onTimerTick(0, 1);
    QVERIFY(renderSpy.count() > 0);
    QVERIFY(endSpy.count() > 0);
    fc.onTimerStep(0, 1);
    delete window;
}

void FrameControllerTest::testOnFrameDecoded() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 2, decoder);
    fc.onFrameDecoded(false); // Should call ErrorReporter
    fc.onFrameDecoded(true);
    delete window;
}

void FrameControllerTest::testOnFrameUploaded() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 3, decoder);
    fc.onFrameUploaded();
    delete window;
}

void FrameControllerTest::testOnFrameRendered() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 4, decoder);
    fc.onFrameRendered();
    delete window;
}

void FrameControllerTest::testOnSeekAndFrameSeeked() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 5, decoder);
    fc.onSeek(0);
    fc.onFrameSeeked(0);
    delete window;
}

void FrameControllerTest::testOnRenderError() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 6, decoder);
    fc.onRenderError();
    delete window;
}

void FrameControllerTest::testClearStall() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 7, decoder);
    auto fq = fc.getFrameQueue();
    QSignalSpy stallSpy(&fc, SIGNAL(decoderStalled(int, bool)));
    // Start controller to set up state
    fc.start();
    QCoreApplication::processEvents();
    // Disable prefill by simulating a frame upload (prefill is set false in onFrameUploaded)
    fc.onFrameUploaded();
    QCoreApplication::processEvents();
    // Request PTS 1, which is not present in the queue (only PTS 0 exists)
    int missingPts = 1; // Now valid since totalFrames is 4
    QVERIFY(fq->getHeadFrame(missingPts) == nullptr);
    fc.onTimerTick(missingPts, 1);
    QCoreApplication::processEvents();
    QVERIFY(stallSpy.count() > 0);
    // Simulate decoder catching up (frame present)
    fq->getTailFrame(missingPts)->setPts(missingPts);
    fc.onFrameDecoded(true); // Should clear stall
    delete window;
}

void FrameControllerTest::testGetTimeBaseAndMeta() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 8, decoder);
    // getTimeBase should return FrameMeta's timeBase
    AVRational tb = fc.getTimeBase();
    QCOMPARE(tb.num, 1);
    QCOMPARE(tb.den, 1);
    // totalFrames and getDuration should match stub meta
    QCOMPARE(fc.totalFrames(), 4);
    QCOMPARE(fc.getDuration(), 4);
    delete window;
}

void FrameControllerTest::testErrorPaths() {
    AppConfig::instance().setQueueSize(4);
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    StubDecoder* decoder = new StubDecoder();
    FrameController fc(nullptr, info, 9, decoder);
    QSignalSpy endSpy(&fc, SIGNAL(endOfVideo(bool, int)));
    fc.onTimerStep(-1, 1);
    QVERIFY(endSpy.count() > 0);
    // Test onFrameSeeked with null frame
    fc.onFrameSeeked(999);
    QVERIFY(endSpy.count() > 1);
    delete window;
}

QTEST_MAIN(FrameControllerTest)
#include "test_framecontroller.moc"
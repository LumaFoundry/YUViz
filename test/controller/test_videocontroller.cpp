#include <QtTest>
#include <memory>
#include "controller/compareController.h"
#include "controller/frameController.h"
#include "controller/timer.h"
#include "controller/videoController.h"
#include "frames/frameMeta.h"
#include "frames/frameQueue.h"
#include "rendering/videoRenderer.h"
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
        meta.setTotalFrames(10);
        meta.setDuration(1000);
        meta.setYWidth(2);
        meta.setYHeight(2);
        meta.setUVWidth(1);
        meta.setUVHeight(1);
        meta.setTimeBase({1, 1});
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
    info.width = 2;
    info.height = 2;
    info.framerate = 30.0;
    info.pixelFormat = AV_PIX_FMT_YUV420P;
    info.forceSoftwareDecoding = false;
    windowOut = new StubVideoWindow();
    info.windowPtr = windowOut;
    return info;
}

class StubFrameController : public FrameController {
    Q_OBJECT
  public:
    std::shared_ptr<FrameMeta> m_testMeta;
    StubFrameController(QObject* parent, VideoFileInfo info, int index) :
        FrameController(parent, info, index, nullptr) {
        m_testMeta = std::make_shared<FrameMeta>();
        m_testMeta->setTotalFrames(10);
        m_testMeta->setDuration(1000);
        m_testMeta->setYWidth(2);
        m_testMeta->setYHeight(2);
        m_testMeta->setUVWidth(1);
        m_testMeta->setUVHeight(1);
        m_testMeta->setTimeBase({1, 1});
    }
    AVRational getTimeBase() override { return AVRational{1, 1}; }
    int64_t getDuration() override { return 1000; }
    int totalFrames() override { return 10; }
    std::shared_ptr<FrameMeta> getFrameMeta() const override { return m_testMeta; }
};

class StubCompareController : public CompareController {
    Q_OBJECT
  public:
    StubCompareController(QObject* parent = nullptr) :
        CompareController(parent) {}
};

class VideoControllerTest : public QObject {
    Q_OBJECT
  private slots:
    void testConstructorAndAddVideo();
    void testPlayPauseSignals();
    void testRemoveVideo();
    void testRemoveVideoInvalidIndex();
    void testSeekAndJump();
    void testStepAndToggle();
    void testSetDiffMode();
    void testOnFCSignals();
    void testMultipleVideosAndFrameNumber();
    void testPauseWhileNotPlaying();
    void testPlayAtEnd();
    void testStepForwardWhilePlaying();
    void testStepForwardAtEnd();
    void testStepBackwardWhilePlaying();
    void testStartMethod();
    void testOnStepMethod();
    void testOnTickMethodBothDirections();
};

void VideoControllerTest::testConstructorAndAddVideo() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    QVERIFY(vc.duration() >= 0);
    QVERIFY(vc.totalFrames() >= 0);
    QVERIFY(!vc.isPlaying());
    QVERIFY(!vc.ready());
    vc.addVideo(info);
    QVERIFY(vc.totalFrames() > 0);
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testPlayPauseSignals() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    QSignalSpy playSpy(&vc, SIGNAL(isPlayingChanged()));
    vc.onReady(0); // Simulate all FCs are ready
    vc.play();
    QCoreApplication::processEvents();
    QVERIFY(playSpy.count() > 0);
    QSignalSpy pauseSpy(&vc, SIGNAL(isPlayingChanged()));
    vc.pause();
    QCoreApplication::processEvents();
    QVERIFY(pauseSpy.count() > 0);
    vc.toggleDirection();
    vc.play();
    QCoreApplication::processEvents();
    QVERIFY(playSpy.count() > 0);
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testRemoveVideo() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.addVideo(info);
    int before = vc.totalFrames();
    vc.removeVideo(0);
    QVERIFY(vc.totalFrames() <= before);
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testRemoveVideoInvalidIndex() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.removeVideo(-1); // Should not crash
    vc.removeVideo(99); // Should not crash
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testSeekAndJump() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    vc.seekTo(500.0);
    vc.jumpToFrame(5);
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testStepAndToggle() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    vc.stepForward();
    vc.stepBackward();
    vc.togglePlayPause();
    vc.toggleDirection();
    vc.setSpeed(2.0f);
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testSetDiffMode() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window1 = nullptr;
    StubVideoWindow* window2 = nullptr;
    VideoFileInfo info1 = makeStubVideoFileInfo(window1);
    VideoFileInfo info2 = makeStubVideoFileInfo(window2);
    std::vector<VideoFileInfo> videoFiles = {info1, info2};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    vc.onReady(1);
    vc.setDiffMode(true, 0, 1);  // Valid
    vc.setDiffMode(false, 0, 1); // Valid
    vc.setDiffMode(true, -1, 1); // Invalid
    vc.setDiffMode(true, 0, 99); // Invalid
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window1;
    delete window2;
}

void VideoControllerTest::testOnFCSignals() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    vc.onFCStartOfVideo(0);
    vc.onFCEndOfVideo(true, 0);
    vc.onFCEndOfVideo(false, 0);
    vc.onSeekCompleted(0);
    vc.onDecoderStalled(0, true);
    vc.onDecoderStalled(0, false);
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testMultipleVideosAndFrameNumber() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window1 = nullptr;
    StubVideoWindow* window2 = nullptr;
    VideoFileInfo info1 = makeStubVideoFileInfo(window1);
    VideoFileInfo info2 = makeStubVideoFileInfo(window2);
    std::vector<VideoFileInfo> videoFiles = {info1, info2};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    vc.onReady(1);
    QVERIFY(vc.frameNumberForTime(0.0) == 0);
    QVERIFY(vc.frameNumberForTime(1000.0) >= 0);
    QVERIFY(vc.frameNumberForTime(-100.0) == 0);
    QVERIFY(vc.frameNumberForTime(999999.0) >= 0);
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window1;
    delete window2;
}

void VideoControllerTest::testPauseWhileNotPlaying() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);

    QSignalSpy pauseSpy(&vc, SIGNAL(isPlayingChanged()));
    vc.pause();
    QCoreApplication::processEvents();
    QVERIFY(pauseSpy.count() > 0);
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testPlayAtEnd() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    // Simulate reaching end by calling onFCEndOfVideo for all FCs
    vc.onFCEndOfVideo(true, 0);
    // Timer is paused after end, so play should trigger seekTo and pendingPlay logic
    vc.play();
    QCoreApplication::processEvents();
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testStepForwardWhilePlaying() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    // Start playing
    vc.play();
    QCoreApplication::processEvents();
    // Step forward while playing
    vc.stepForward();
    QCoreApplication::processEvents();
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testStepForwardAtEnd() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    // Simulate reaching end
    vc.onFCEndOfVideo(true, 0);
    // Try to step forward at end
    vc.stepForward();
    QCoreApplication::processEvents();
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testStepBackwardWhilePlaying() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window = nullptr;
    VideoFileInfo info = makeStubVideoFileInfo(window);
    std::vector<VideoFileInfo> videoFiles = {info};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    // Start playing
    vc.play();
    QCoreApplication::processEvents();
    // Step backward while playing
    vc.stepBackward();
    QCoreApplication::processEvents();
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window;
}

void VideoControllerTest::testStartMethod() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window1 = nullptr;
    StubVideoWindow* window2 = nullptr;
    VideoFileInfo info1 = makeStubVideoFileInfo(window1);
    VideoFileInfo info2 = makeStubVideoFileInfo(window2);
    std::vector<VideoFileInfo> videoFiles = {info1, info2};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    vc.onReady(1);
    vc.start(); // Should call start on all FCs
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window1;
    delete window2;
}

void VideoControllerTest::testOnStepMethod() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window1 = nullptr;
    StubVideoWindow* window2 = nullptr;
    VideoFileInfo info1 = makeStubVideoFileInfo(window1);
    VideoFileInfo info2 = makeStubVideoFileInfo(window2);
    std::vector<VideoFileInfo> videoFiles = {info1, info2};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    vc.onReady(1);
    std::vector<int64_t> pts = {0, 0};
    std::vector<bool> update = {true, true};
    int64_t playingTimeMs = 0;
    vc.onStep(pts, update, playingTimeMs);
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window1;
    delete window2;
}

void VideoControllerTest::testOnTickMethodBothDirections() {
    VideoController::s_testFrameControllerFactory = [](QObject* parent, VideoFileInfo info, int index) {
        StubVideoWindow* window = nullptr;
        VideoFileInfo stubInfo = makeStubVideoFileInfo(window);
        auto decoder = new StubDecoder();
        return std::make_unique<FrameController>(parent, stubInfo, index, decoder);
    };
    auto compareController = std::make_shared<StubCompareController>();
    StubVideoWindow* window1 = nullptr;
    StubVideoWindow* window2 = nullptr;
    VideoFileInfo info1 = makeStubVideoFileInfo(window1);
    VideoFileInfo info2 = makeStubVideoFileInfo(window2);
    std::vector<VideoFileInfo> videoFiles = {info1, info2};
    VideoController vc(nullptr, compareController, videoFiles);
    vc.onReady(0);
    vc.onReady(1);
    std::vector<int64_t> pts = {0, 1};
    std::vector<bool> update = {true, true};
    int64_t playingTimeMs = 123;
    // Forward direction
    vc.onTick(pts, update, playingTimeMs);
    // Backward direction
    vc.toggleDirection(); // sets direction to -1
    vc.onTick(pts, update, playingTimeMs);
    VideoController::s_testFrameControllerFactory = nullptr;
    delete window1;
    delete window2;
}

QTEST_MAIN(VideoControllerTest)

#include "test_videocontroller.moc"

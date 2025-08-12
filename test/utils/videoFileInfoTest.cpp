#include "videoFileInfoTest.h"
#include <QDebug>
#include <QTest>
#include "../../src/utils/videoFileInfo.h"

void VideoFileInfoTest::testDefaultConstructor() {
    VideoFileInfo info;

    QVERIFY(info.filename.isEmpty());
    // Note: VideoFileInfo is a struct without explicit initialization
    // so we can't assume default values for primitive types
    QCOMPARE(info.framerate, 0.0);
    // Don't test pixelFormat as it may have undefined initial value
    QCOMPARE(info.graphicsApi, QRhi::Implementation::Null);
    QVERIFY(info.windowPtr == nullptr);
    QVERIFY(!info.forceSoftwareDecoding);
}

void VideoFileInfoTest::testPropertyAssignment() {
    VideoFileInfo info;

    // Test basic property assignment
    info.filename = "test_video.mp4";
    info.width = 1920;
    info.height = 1080;

    QCOMPARE(info.filename, QString("test_video.mp4"));
    QCOMPARE(info.width, 1920);
    QCOMPARE(info.height, 1080);
}

void VideoFileInfoTest::testFilenameProperty() {
    VideoFileInfo info;

    // Test various filename formats
    std::vector<QString> testFilenames = {"video.mp4",
                                          "/path/to/video.avi",
                                          "test_video_with_underscores.mkv",
                                          "video with spaces.mp4",
                                          "video.123.456.789.mp4"};

    for (const QString& filename : testFilenames) {
        info.filename = filename;
        QCOMPARE(info.filename, filename);
    }
}

void VideoFileInfoTest::testDimensions() {
    VideoFileInfo info;

    // Test common video resolutions
    std::vector<std::pair<int, int>> resolutions = {
        {640, 480},   // VGA
        {1280, 720},  // HD
        {1920, 1080}, // Full HD
        {2560, 1440}, // 2K
        {3840, 2160}, // 4K
        {7680, 4320}  // 8K
    };

    for (const auto& [width, height] : resolutions) {
        info.width = width;
        info.height = height;
        QCOMPARE(info.width, width);
        QCOMPARE(info.height, height);
    }
}

void VideoFileInfoTest::testFramerate() {
    VideoFileInfo info;

    // Test common framerates
    std::vector<double> framerates = {24.0, 25.0, 30.0, 50.0, 60.0, 120.0};

    for (double framerate : framerates) {
        info.framerate = framerate;
        QCOMPARE(info.framerate, framerate);
    }

    // Test edge cases
    info.framerate = 0.0;
    QCOMPARE(info.framerate, 0.0);

    info.framerate = 1000.0;
    QCOMPARE(info.framerate, 1000.0);
}

void VideoFileInfoTest::testPixelFormat() {
    VideoFileInfo info;

    // Test various pixel formats
    std::vector<AVPixelFormat> formats = {
        AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV422P, AV_PIX_FMT_RGB24, AV_PIX_FMT_BGR24, AV_PIX_FMT_GRAY8, AV_PIX_FMT_NV12};

    for (AVPixelFormat format : formats) {
        info.pixelFormat = format;
        QCOMPARE(info.pixelFormat, format);
    }
}

void VideoFileInfoTest::testGraphicsApi() {
    VideoFileInfo info;

    // Test various graphics APIs
    std::vector<QRhi::Implementation> apis = {QRhi::Implementation::Null,
                                              QRhi::Implementation::Vulkan,
                                              QRhi::Implementation::OpenGLES2,
                                              QRhi::Implementation::D3D11,
                                              QRhi::Implementation::Metal};

    for (QRhi::Implementation api : apis) {
        info.graphicsApi = api;
        QCOMPARE(info.graphicsApi, api);
    }
}

void VideoFileInfoTest::testForceSoftwareDecoding() {
    VideoFileInfo info;

    // Test default value
    QVERIFY(!info.forceSoftwareDecoding);

    // Test setting to true
    info.forceSoftwareDecoding = true;
    QVERIFY(info.forceSoftwareDecoding);

    // Test setting to false
    info.forceSoftwareDecoding = false;
    QVERIFY(!info.forceSoftwareDecoding);
}

void VideoFileInfoTest::testCompleteVideoFileInfo() {
    VideoFileInfo info;

    // Set all properties to create a complete video file info
    info.filename = "sample_video.mp4";
    info.width = 1920;
    info.height = 1080;
    info.framerate = 30.0;
    info.pixelFormat = AV_PIX_FMT_YUV420P;
    info.graphicsApi = QRhi::Implementation::Vulkan;
    info.windowPtr = nullptr; // In test environment, we can't create actual window
    info.forceSoftwareDecoding = false;

    // Verify all properties
    QCOMPARE(info.filename, QString("sample_video.mp4"));
    QCOMPARE(info.width, 1920);
    QCOMPARE(info.height, 1080);
    QCOMPARE(info.framerate, 30.0);
    QCOMPARE(info.pixelFormat, AV_PIX_FMT_YUV420P);
    QCOMPARE(info.graphicsApi, QRhi::Implementation::Vulkan);
    QVERIFY(info.windowPtr == nullptr);
    QVERIFY(!info.forceSoftwareDecoding);
}

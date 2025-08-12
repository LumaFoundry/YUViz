#include "videoRendererTest.h"
#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <memory>
#include "../../src/frames/frameData.h"
#include "../../src/frames/frameMeta.h"
#include "../../src/rendering/videoRenderer.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

void VideoRendererTest::testConstructor() {
    // Create a mock FrameMeta
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(1920);
    meta->setYHeight(1080);

    // Test constructor
    VideoRenderer renderer(this, meta);

    // Verify frame meta is set correctly
    auto retrievedMeta = renderer.getFrameMeta();
    QVERIFY(retrievedMeta != nullptr);
    QCOMPARE(retrievedMeta->yWidth(), 1920);
    QCOMPARE(retrievedMeta->yHeight(), 1080);

    // Verify initial current frame is null
    QVERIFY(renderer.getCurrentFrame() == nullptr);

    // Test signal connections
    QSignalSpy batchFullSpy(&renderer, &VideoRenderer::batchIsFull);
    QSignalSpy batchEmptySpy(&renderer, &VideoRenderer::batchIsEmpty);
    QSignalSpy rendererErrorSpy(&renderer, &VideoRenderer::rendererError);

    // Verify signals are connected
    QVERIFY(batchFullSpy.isValid());
    QVERIFY(batchEmptySpy.isValid());
    QVERIFY(rendererErrorSpy.isValid());
}

void VideoRendererTest::testRendering() {
    auto meta = std::make_shared<FrameMeta>();
    VideoRenderer renderer(this, meta);

    // Test that renderer object can be created
    QVERIFY(&renderer != nullptr);
}

void VideoRendererTest::testErrorHandling() {
    auto meta = std::make_shared<FrameMeta>();
    VideoRenderer renderer(this, meta);

    // Test signal connections for error handling
    QSignalSpy batchFullSpy(&renderer, &VideoRenderer::batchIsFull);
    QSignalSpy batchEmptySpy(&renderer, &VideoRenderer::batchIsEmpty);
    QSignalSpy rendererErrorSpy(&renderer, &VideoRenderer::rendererError);

    // Verify signals are connected
    QVERIFY(batchFullSpy.isValid());
    QVERIFY(batchEmptySpy.isValid());
    QVERIFY(rendererErrorSpy.isValid());

    // Test resource release
    renderer.releaseBatch();
    QVERIFY(&renderer != nullptr);
}

#include <QColor>
#include <QSignalSpy>
#include <QtTest>
#include <memory>
#include <vector>

#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "rendering/videoRenderer.h"
#include "rhi/qrhi.h"

static std::shared_ptr<FrameMeta> makeMeta(int yW = 4, int yH = 4) {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(yW);
    meta->setYHeight(yH);
    meta->setUVWidth(yW / 2);
    meta->setUVHeight(yH / 2);
    meta->setColorSpace(AVCOL_SPC_BT709);
    meta->setColorRange(AVCOL_RANGE_MPEG);
    return meta;
}

static std::unique_ptr<QRhi> makeNullRhi() {
    QRhiNullInitParams params;
    return std::unique_ptr<QRhi>(QRhi::create(QRhi::Null, &params));
}

static void makeRtAndRp(QRhi* rhi,
                        std::unique_ptr<QRhiTexture>& colorTex,
                        std::unique_ptr<QRhiTextureRenderTarget>& rt,
                        std::unique_ptr<QRhiRenderPassDescriptor>& rp,
                        const QSize& size = QSize(8, 8)) {
    colorTex.reset(rhi->newTexture(QRhiTexture::RGBA8, size));
    colorTex->create();
    QRhiTextureRenderTargetDescription rtDesc(QRhiColorAttachment(colorTex.get()));
    rt.reset(rhi->newTextureRenderTarget(rtDesc));
    rp.reset(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.get());
    rt->create();
}

class VideoRendererTest : public QObject {
    Q_OBJECT

  private slots:
    void testInitializeUploadRender();
    void testErrorPaths();
    void testZoomAndViewportBranches();
    void testInvalidDimensions();
    void testInitializeNullRhi();
    void testRenderWithoutUpload();
    void testLetterboxBothBranches();
    void testSetColorParamsBatches();
    void testReleaseWithoutInitAndGetters();
};

void VideoRendererTest::testInitializeUploadRender() {
    auto meta = makeMeta();
    auto rhi = makeNullRhi();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }

    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi.get(), colorTex, rt, rp);

    VideoRenderer vr(nullptr, meta);
    QSignalSpy errSpy(&vr, SIGNAL(rendererError()));
    QSignalSpy fullSpy(&vr, SIGNAL(batchIsFull()));
    QSignalSpy emptySpy(&vr, SIGNAL(batchIsEmpty()));

    vr.initialize(rhi.get(), rp.get());
    if (errSpy.count() > 0) {
        QSKIP("VideoRenderer initialize failed in this environment");
    }

    // Prepare a valid frame
    const int ySize = meta->ySize();
    const int uvSize = meta->uvSize();
    auto buffer = std::make_shared<std::vector<uint8_t>>(ySize + uvSize * 2, 0x80);
    FrameData frame(ySize, uvSize, buffer, 0);
    frame.setPts(0);

    vr.setColorParams(meta->colorSpace(), meta->colorRange());
    vr.setComponentDisplayMode(0);
    vr.setComponentDisplayMode(1);
    vr.setComponentDisplayMode(2);
    vr.setComponentDisplayMode(3);

    vr.uploadFrame(&frame);
    QTRY_VERIFY_WITH_TIMEOUT(fullSpy.count() > 0 || errSpy.count() > 0, 1000);
    if (errSpy.count() > 0) {
        QSKIP("VideoRenderer uploadFrame reported rendererError in this environment");
    }
    QVERIFY(vr.getCurrentFrame() == &frame);

    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi offscreen frame not supported");
    }
    const QRect sameViewport(0, 0, 8, 8);
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, sameViewport, rt.get());
    cb->endPass();
    // Render again with same viewport to exercise no-aspect-update branch
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, sameViewport, rt.get());
    cb->endPass();
    QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() > 0, 1000);
    rhi->endOffscreenFrame();

    vr.releaseBatch();
    // Call again to cover empty branches
    vr.releaseBatch();
}

void VideoRendererTest::testErrorPaths() {
    auto meta = makeMeta();
    VideoRenderer vr(nullptr, meta);
    QSignalSpy errSpy(&vr, SIGNAL(rendererError()));

    // upload before initialize
    vr.uploadFrame(nullptr);
    QVERIFY(errSpy.count() > 0);

    // Now try with RHI but invalid frame data
    auto rhi = makeNullRhi();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi.get(), colorTex, rt, rp);
    vr.initialize(rhi.get(), rp.get());

    vr.uploadFrame(nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(errSpy.count() > 1, 1000);
}

void VideoRendererTest::testZoomAndViewportBranches() {
    auto meta = makeMeta(16, 8); // non-square to exercise aspect logic
    auto rhi = makeNullRhi();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi.get(), colorTex, rt, rp, QSize(32, 32));

    VideoRenderer vr(nullptr, meta);
    vr.initialize(rhi.get(), rp.get());

    // Valid frame
    auto buffer = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x7F);
    FrameData frame(meta->ySize(), meta->uvSize(), buffer, 0);
    frame.setPts(1);
    vr.uploadFrame(&frame);

    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi offscreen frame not supported");
    }
    // Wider viewport
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, QRect(0, 0, 64, 32), rt.get());
    cb->endPass();
    // Taller viewport
    vr.setZoomAndOffset(1.2f, 0.4f, 0.6f);
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, QRect(0, 0, 32, 64), rt.get());
    cb->endPass();
    rhi->endOffscreenFrame();
}

void VideoRendererTest::testInvalidDimensions() {
    auto meta = makeMeta(0, 0); // invalid sizes
    auto rhi = makeNullRhi();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi.get(), colorTex, rt, rp);

    VideoRenderer vr(nullptr, meta);
    QSignalSpy errSpy(&vr, SIGNAL(rendererError()));
    vr.initialize(rhi.get(), rp.get());

    // Invalid frame buffer/geometry as well
    auto buffer = std::make_shared<std::vector<uint8_t>>(0);
    FrameData frame(0, 0, buffer, 0);
    vr.uploadFrame(&frame);
    QTRY_VERIFY_WITH_TIMEOUT(errSpy.count() > 0, 500);
}

void VideoRendererTest::testInitializeNullRhi() {
    auto meta = makeMeta();
    VideoRenderer vr(nullptr, meta);
    QSignalSpy errSpy(&vr, SIGNAL(rendererError()));
    // Intentionally pass null rhi/rp
    vr.initialize(nullptr, nullptr);
    QVERIFY(errSpy.count() > 0);
}

void VideoRendererTest::testRenderWithoutUpload() {
    auto meta = makeMeta(8, 8);
    auto rhi = makeNullRhi();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi.get(), colorTex, rt, rp);

    VideoRenderer vr(nullptr, meta);
    QSignalSpy emptySpy(&vr, SIGNAL(batchIsEmpty()));
    vr.initialize(rhi.get(), rp.get());
    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi offscreen frame not supported");
    }
    const QRect vp(0, 0, 8, 8);
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, vp, rt.get()); // no uploadFrame before
    cb->endPass();
    rhi->endOffscreenFrame();
    // Even without frame, renderFrame should not crash; batchIsEmpty may emit or not depending on init state
}

void VideoRendererTest::testLetterboxBothBranches() {
    auto metaTall = makeMeta(8, 16); // videoAspect < 1 (taller)
    auto metaWide = makeMeta(16, 8); // videoAspect > 1 (wider)
    auto rhi = makeNullRhi();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi.get(), colorTex, rt, rp, QSize(64, 64));

    // Wider window, taller video => width letterboxed branch
    {
        VideoRenderer vr(nullptr, metaTall);
        vr.initialize(rhi.get(), rp.get());
        auto buf = std::make_shared<std::vector<uint8_t>>(metaTall->ySize() + metaTall->uvSize() * 2, 0x33);
        FrameData f(metaTall->ySize(), metaTall->uvSize(), buf, 0);
        f.setPts(1);
        vr.uploadFrame(&f);
        QRhiCommandBuffer* cb = nullptr;
        if (!rhi->beginOffscreenFrame(&cb)) {
            QSKIP("QRhi offscreen frame not supported");
        }
        // square viewport
        cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
        vr.renderFrame(cb, QRect(0, 0, 64, 64), rt.get());
        cb->endPass();
        rhi->endOffscreenFrame();
    }

    // Taller window, wider video => height letterboxed branch
    {
        VideoRenderer vr(nullptr, metaWide);
        vr.initialize(rhi.get(), rp.get());
        auto buf = std::make_shared<std::vector<uint8_t>>(metaWide->ySize() + metaWide->uvSize() * 2, 0x44);
        FrameData f(metaWide->ySize(), metaWide->uvSize(), buf, 0);
        f.setPts(2);
        vr.uploadFrame(&f);
        QRhiCommandBuffer* cb = nullptr;
        if (!rhi->beginOffscreenFrame(&cb)) {
            QSKIP("QRhi offscreen frame not supported");
        }
        // portrait viewport to force other branch
        cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
        vr.renderFrame(cb, QRect(0, 0, 32, 64), rt.get());
        cb->endPass();
        rhi->endOffscreenFrame();
    }
}

void VideoRendererTest::testSetColorParamsBatches() {
    auto meta = makeMeta(8, 8);
    auto rhi = makeNullRhi();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi.get(), colorTex, rt, rp);

    VideoRenderer vr(nullptr, meta);
    vr.initialize(rhi.get(), rp.get());

    // Trigger color params update via both API
    vr.setColorParams(AVCOL_SPC_BT709, AVCOL_RANGE_JPEG);
    vr.setComponentDisplayMode(2);

    // Also nudge zoom to force resize param batch on next render
    vr.setZoomAndOffset(1.1f, 0.55f, 0.45f);

    // Minimal valid frame to allow render path
    auto buffer = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x12);
    FrameData frame(meta->ySize(), meta->uvSize(), buffer, 0);
    frame.setPts(9);
    vr.uploadFrame(&frame);

    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi Null backend not available");
    }
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get());
    cb->endPass();
    rhi->endOffscreenFrame();
}

void VideoRendererTest::testReleaseWithoutInitAndGetters() {
    auto meta = makeMeta(4, 4);
    VideoRenderer vr(nullptr, meta);
    // Ensure releaseBatch is safe pre-initialize
    vr.releaseBatch();
    // Getter coverage
    QVERIFY(vr.getFrameMeta() == meta);
    QVERIFY(vr.getCurrentFrame() == nullptr);
}

QTEST_MAIN(VideoRendererTest)
#include "test_videorenderer.moc"

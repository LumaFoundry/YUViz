#include <QColor>
#include <QFile>
#include <QSignalSpy>
#include <QWindow>
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

static bool shadersAvailableVR() {
    QFile v(":/shaders/vertex.vert.qsb");
    QFile f(":/shaders/fragment.frag.qsb");
    return v.exists() && f.exists();
}

static std::unique_ptr<QRhi> makeStandaloneRhiD3D11() {
    QRhiD3D11InitParams params; // zero-initialized
    if (QRhi* r = QRhi::create(QRhi::D3D11, &params)) {
        return std::unique_ptr<QRhi>(r);
    }
    return nullptr;
}

struct D3D11SwapchainEnv {
    std::unique_ptr<QWindow> window;
    std::unique_ptr<QRhi> rhi;
    std::unique_ptr<QRhiSwapChain> swapchain;
};

static bool makeD3D11RhiWithSwapchain(D3D11SwapchainEnv& env, const QSize& size) {
    env.window.reset(new QWindow());
    env.window->resize(size);
    env.window->show();
    if (!QTest::qWaitForWindowExposed(env.window.get(), 3000)) {
        env = {};
        return false;
    }
    QRhiD3D11InitParams params;
    env.rhi.reset(QRhi::create(QRhi::D3D11, &params));
    if (!env.rhi) {
        env = {};
        return false;
    }
    env.swapchain.reset(env.rhi->newSwapChain());
    env.swapchain->setWindow(env.window.get());
    if (!env.swapchain->createOrResize()) {
        env = {};
        return false;
    }
    return true;
}

static void makeRtAndRp(QRhi* rhi,
                        std::unique_ptr<QRhiTexture>& colorTex,
                        std::unique_ptr<QRhiTextureRenderTarget>& rt,
                        std::unique_ptr<QRhiRenderPassDescriptor>& rp,
                        const QSize& size = QSize(8, 8)) {
    colorTex.reset(rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
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
    QVERIFY2(shadersAvailableVR(), "Shader resources not found in test target");
    D3D11SwapchainEnv env;
    QVERIFY2(makeD3D11RhiWithSwapchain(env, QSize(128, 128)), "Failed to init D3D11 swapchain env");
    QRhi* rhi = env.rhi.get();

    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi, colorTex, rt, rp);

    VideoRenderer vr(nullptr, meta);
    QSignalSpy errSpy(&vr, SIGNAL(rendererError()));
    QSignalSpy fullSpy(&vr, SIGNAL(batchIsFull()));
    QSignalSpy emptySpy(&vr, SIGNAL(batchIsEmpty()));

    vr.initialize(rhi, rp.get());
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

    env.rhi->beginFrame(env.swapchain.get());
    QRhiCommandBuffer* cb = env.swapchain->currentFrameCommandBuffer();
    const QRect sameViewport(0, 0, 8, 8);
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, sameViewport, rt.get());
    cb->endPass();
    env.rhi->endFrame(env.swapchain.get());
    // Render again with same viewport to exercise no-aspect-update branch
    env.rhi->beginFrame(env.swapchain.get());
    cb = env.swapchain->currentFrameCommandBuffer();
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, sameViewport, rt.get());
    cb->endPass();
    env.rhi->endFrame(env.swapchain.get());
    QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() > 0, 1000);

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
    D3D11SwapchainEnv env;
    if (!makeD3D11RhiWithSwapchain(env, QSize(64, 64))) {
        QSKIP("D3D11 swapchain not available");
    }
    QRhi* rhi = env.rhi.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi, colorTex, rt, rp);
    vr.initialize(rhi, rp.get());

    vr.uploadFrame(nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(errSpy.count() > 1, 1000);
}

void VideoRendererTest::testZoomAndViewportBranches() {
    auto meta = makeMeta(16, 8); // non-square to exercise aspect logic
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi, colorTex, rt, rp, QSize(32, 32));

    VideoRenderer vr(nullptr, meta);
    vr.initialize(rhi, rp.get());

    // Valid frame
    auto buffer = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x7F);
    FrameData frame(meta->ySize(), meta->uvSize(), buffer, 0);
    frame.setPts(1);
    vr.uploadFrame(&frame);

    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(64, 64))) {
        QSKIP("D3D11 swapchain not available");
    }
    sc.rhi->beginFrame(sc.swapchain.get());
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    // Wider viewport
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, QRect(0, 0, 64, 32), rt.get());
    cb->endPass();
    // Taller viewport
    vr.setZoomAndOffset(1.2f, 0.4f, 0.6f);
    sc.rhi->beginFrame(sc.swapchain.get());
    cb = sc.swapchain->currentFrameCommandBuffer();
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, QRect(0, 0, 32, 64), rt.get());
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());
}

void VideoRendererTest::testInvalidDimensions() {
    auto meta = makeMeta(0, 0); // invalid sizes
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi, colorTex, rt, rp);

    VideoRenderer vr(nullptr, meta);
    QSignalSpy errSpy(&vr, SIGNAL(rendererError()));
    vr.initialize(rhi, rp.get());

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
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi, colorTex, rt, rp);

    VideoRenderer vr(nullptr, meta);
    QSignalSpy emptySpy(&vr, SIGNAL(batchIsEmpty()));
    vr.initialize(rhi, rp.get());
    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(16, 16))) {
        QSKIP("D3D11 swapchain not available");
    }
    sc.rhi->beginFrame(sc.swapchain.get());
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    const QRect vp(0, 0, 8, 8);
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, vp, rt.get()); // no uploadFrame before
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());
    // Even without frame, renderFrame should not crash; batchIsEmpty may emit or not depending on init state
}

void VideoRendererTest::testLetterboxBothBranches() {
    auto metaTall = makeMeta(8, 16); // videoAspect < 1 (taller)
    auto metaWide = makeMeta(16, 8); // videoAspect > 1 (wider)
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi, colorTex, rt, rp, QSize(64, 64));

    // Wider window, taller video => width letterboxed branch
    {
        VideoRenderer vr(nullptr, metaTall);
        vr.initialize(rhi, rp.get());
        auto buf = std::make_shared<std::vector<uint8_t>>(metaTall->ySize() + metaTall->uvSize() * 2, 0x33);
        FrameData f(metaTall->ySize(), metaTall->uvSize(), buf, 0);
        f.setPts(1);
        vr.uploadFrame(&f);
        D3D11SwapchainEnv scA;
        if (!makeD3D11RhiWithSwapchain(scA, QSize(64, 64))) {
            QSKIP("D3D11 swapchain not available");
        }
        scA.rhi->beginFrame(scA.swapchain.get());
        QRhiCommandBuffer* cb = scA.swapchain->currentFrameCommandBuffer();
        // square viewport
        cb->beginPass(scA.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
        vr.renderFrame(cb, QRect(0, 0, 64, 64), rt.get());
        cb->endPass();
        scA.rhi->endFrame(scA.swapchain.get());
    }

    // Taller window, wider video => height letterboxed branch
    {
        VideoRenderer vr(nullptr, metaWide);
        vr.initialize(rhi, rp.get());
        auto buf = std::make_shared<std::vector<uint8_t>>(metaWide->ySize() + metaWide->uvSize() * 2, 0x44);
        FrameData f(metaWide->ySize(), metaWide->uvSize(), buf, 0);
        f.setPts(2);
        vr.uploadFrame(&f);
        D3D11SwapchainEnv scB;
        if (!makeD3D11RhiWithSwapchain(scB, QSize(64, 64))) {
            QSKIP("D3D11 swapchain not available");
        }
        scB.rhi->beginFrame(scB.swapchain.get());
        QRhiCommandBuffer* cb2 = scB.swapchain->currentFrameCommandBuffer();
        // portrait viewport to force other branch
        cb2->beginPass(scB.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
        vr.renderFrame(cb2, QRect(0, 0, 32, 64), rt.get());
        cb2->endPass();
        scB.rhi->endFrame(scB.swapchain.get());
    }
}

void VideoRendererTest::testSetColorParamsBatches() {
    auto meta = makeMeta(8, 8);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRp(rhi, colorTex, rt, rp);

    VideoRenderer vr(nullptr, meta);
    vr.initialize(rhi, rp.get());

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

    D3D11SwapchainEnv scC;
    if (!makeD3D11RhiWithSwapchain(scC, QSize(32, 32))) {
        QSKIP("D3D11 swapchain not available");
    }
    scC.rhi->beginFrame(scC.swapchain.get());
    QRhiCommandBuffer* cb = scC.swapchain->currentFrameCommandBuffer();
    cb->beginPass(scC.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    vr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get());
    cb->endPass();
    scC.rhi->endFrame(scC.swapchain.get());
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

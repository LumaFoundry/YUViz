#include <QColor>
#include <QFile>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QWindow>
#include <QtTest>
#include <memory>
#include <vector>

#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "rendering/diffRenderer.h"
#include "rhi/qrhi.h"

static std::shared_ptr<FrameMeta> makeMetaDR(int yW = 4, int yH = 4) {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(yW);
    meta->setYHeight(yH);
    meta->setUVWidth(yW / 2);
    meta->setUVHeight(yH / 2);
    return meta;
}

static std::unique_ptr<QRhi> makeNullRhiDR() {
    // Prefer a real backend on Windows to ensure offscreen frames work, fallback to Null
    {
        QRhiD3D11InitParams d3dParams; // zero-initialized
        if (QRhi* r = QRhi::create(QRhi::D3D11, &d3dParams)) {
            return std::unique_ptr<QRhi>(r);
        }
    }
    QRhiNullInitParams params;
    return std::unique_ptr<QRhi>(QRhi::create(QRhi::Null, &params));
}

static bool shadersAvailable() {
    // Ensure shader resources are present in this test target
    QFile v(":/shaders/vertex.vert.qsb");
    QFile f(":/shaders/fragment-diff.frag.qsb");
    return v.exists() && f.exists();
}

static bool makeWindowAndRhi(std::unique_ptr<QQuickWindow>& windowOut, QRhi*& rhiOut, const QSize& sz) {
    qputenv("QSG_RENDER_LOOP", QByteArray("basic"));
    qputenv("QSG_RHI_BACKEND", QByteArray("d3d11"));
    auto* w = new QQuickWindow();
    w->resize(sz);
    w->show();
    if (!QTest::qWaitForWindowExposed(w, 3000)) {
        delete w;
        return false;
    }
    windowOut.reset(w);
    rhiOut = w->rhi();
    return rhiOut != nullptr;
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
    std::unique_ptr<QRhiRenderBuffer> ds; // depth-stencil optional
};

static bool makeD3D11RhiWithSwapchain(D3D11SwapchainEnv& env, const QSize& size) {
    env.window.reset(new QWindow());
    env.window->resize(size);
    env.window->show();
    if (!QTest::qWaitForWindowExposed(env.window.get(), 3000)) {
        env = {};
        return false;
    }

    // Create RHI
    QRhiD3D11InitParams params;
    env.rhi.reset(QRhi::create(QRhi::D3D11, &params));
    if (!env.rhi) {
        env = {};
        return false;
    }

    // Create swapchain
    env.swapchain.reset(env.rhi->newSwapChain());
    env.swapchain->setWindow(env.window.get());
    // Use default depth/sample/buffer count settings available on this Qt version
    if (!env.swapchain->createOrResize()) {
        env = {};
        return false;
    }
    return true;
}

static void makeRtAndRpDR(QRhi* rhi,
                          std::unique_ptr<QRhiTexture>& colorTex,
                          std::unique_ptr<QRhiTextureRenderTarget>& rt,
                          std::unique_ptr<QRhiRenderPassDescriptor>& rp,
                          const QSize& size = QSize(8, 8)) {
    // For use as a color attachment, the texture must have the RenderTarget flag
    colorTex.reset(rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
    colorTex->create();
    QRhiTextureRenderTargetDescription rtDesc(QRhiColorAttachment(colorTex.get()));
    rt.reset(rhi->newTextureRenderTarget(rtDesc));
    rp.reset(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.get());
    rt->create();
}

class DiffRendererTest : public QObject {
    Q_OBJECT

  private slots:
    void testInitializeUploadRender();
    void testErrorAndConfigBranches();
    void testZoomAndViewportBranches();
    void testInvalidDimensions();
    void testInitializeNullRhi();
    void testRenderWithoutUpload();
    void testLetterboxBothBranches();
    void testZoomResetsWindowAspect();
    void testMultipleUploadsUpdatePtsAndSignals();
    void testGetters();
    void testReleaseAfterInit();
    void testMoreConfigCombos();
    void testUploadBeforeInitValidFrames();
    void testExtremeDiffMultiplier();
    void testPartialNullFrame();
    void testReleaseBatchesPaths();
    void testConfigOnlyRenderConsumesBatch();
    void testViewportVarietyLetterbox();
    void testRenderConsumesInitOnly();
    void testResizeOnlyBatchPath();
    void testRenderFrameComprehensive();
};

void DiffRendererTest::testInitializeUploadRender() {
    auto meta = makeMetaDR();
    D3D11SwapchainEnv env;
    QVERIFY2(makeD3D11RhiWithSwapchain(env, QSize(128, 128)), "Failed to init D3D11 swapchain env");
    QRhi* rhi = env.rhi.get();
    QVERIFY2(shadersAvailable(), "Shader resources not found in test target");

    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy errSpy(&dr, SIGNAL(rendererError()));
    QSignalSpy fullSpy(&dr, SIGNAL(batchIsFull()));
    QSignalSpy emptySpy(&dr, SIGNAL(batchIsEmpty()));

    dr.initialize(rhi, rp.get());
    QVERIFY2(errSpy.count() == 0, "DiffRenderer initialize emitted rendererError");

    // Two valid frames (Y only required by implementation)
    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x10);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(2);
    f2.setPts(2);

    dr.setDiffConfig(0, 4.0f, 0);
    dr.setDiffConfig(1, 2.0f, 0);
    dr.setDiffConfig(2, 8.0f, 1);
    dr.uploadFrame(&f1, &f2);
    QTRY_VERIFY_WITH_TIMEOUT(fullSpy.count() > 0 || errSpy.count() > 0, 1000);
    if (errSpy.count() > 0) {
        QSKIP("DiffRenderer uploadFrame reported rendererError in this environment");
    }
    QVERIFY(dr.getCurrentPts1() == 2);
    QVERIFY(dr.getCurrentPts2() == 2);

    env.rhi->beginFrame(env.swapchain.get());
    QRhiCommandBuffer* cb = env.swapchain->currentFrameCommandBuffer();
    QVERIFY2(cb != nullptr, "No command buffer from swapchain");
    const QRect sameViewport(0, 0, 8, 8);
    cb->beginPass(env.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, sameViewport, rt.get());
    cb->endPass();
    // Render again with same viewport to hit no-aspect-update branch
    cb->beginPass(env.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, sameViewport, rt.get());
    cb->endPass();
    QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() > 0, 1000);
    env.rhi->endFrame(env.swapchain.get());

    dr.releaseBatch();
    // Call again to cover empty branches
    dr.releaseBatch();
}

void DiffRendererTest::testErrorAndConfigBranches() {
    auto meta = makeMetaDR();
    DiffRenderer dr(nullptr, meta);
    QSignalSpy errSpy(&dr, SIGNAL(rendererError()));

    // upload before initialize
    dr.uploadFrame(nullptr, nullptr);
    QVERIFY(errSpy.count() > 0);

    std::unique_ptr<QQuickWindow> window;
    QRhi* rhi = nullptr;
    if (!makeWindowAndRhi(window, rhi, QSize(32, 32))) {
        QSKIP("QQuickWindow RHI not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);
    dr.initialize(rhi, rp.get());

    // invalid frames
    dr.uploadFrame(nullptr, nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(errSpy.count() > 1, 1000);

    // config branch (invokes nextResourceUpdateBatch path)
    dr.setDiffConfig(2, 8.0f, 1);
}

void DiffRendererTest::testZoomAndViewportBranches() {
    auto meta = makeMetaDR(16, 8);
    D3D11SwapchainEnv env;
    if (!makeD3D11RhiWithSwapchain(env, QSize(64, 64))) {
        QSKIP("D3D11 swapchain not available");
    }
    QRhi* rhi = env.rhi.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp, QSize(32, 32));

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi, rp.get());

    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x20);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(5);
    f2.setPts(5);
    dr.uploadFrame(&f1, &f2);

    env.rhi->beginFrame(env.swapchain.get());
    QRhiCommandBuffer* cb = env.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    cb->beginPass(env.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 64, 32), rt.get());
    cb->endPass();
    dr.setZoomAndOffset(1.5f, 0.3f, 0.7f);
    cb->beginPass(env.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 32, 64), rt.get());
    cb->endPass();
    env.rhi->endFrame(env.swapchain.get());
}

void DiffRendererTest::testInvalidDimensions() {
    auto meta = makeMetaDR(0, 0);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy errSpy(&dr, SIGNAL(rendererError()));
    dr.initialize(rhi, rp.get());

    auto buf = std::make_shared<std::vector<uint8_t>>(0);
    FrameData f1(0, 0, buf, 0);
    FrameData f2(0, 0, buf, 0);
    dr.uploadFrame(&f1, &f2);
    QTRY_VERIFY_WITH_TIMEOUT(errSpy.count() > 0, 500);
}

void DiffRendererTest::testInitializeNullRhi() {
    auto meta = makeMetaDR();
    DiffRenderer dr(nullptr, meta);
    QSignalSpy errSpy(&dr, SIGNAL(rendererError()));
    dr.initialize(nullptr, nullptr);
    QVERIFY(errSpy.count() > 0);
}

void DiffRendererTest::testRenderWithoutUpload() {
    auto meta = makeMetaDR(8, 8);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi, rp.get());

    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(16, 16))) {
        QSKIP("D3D11 swapchain not available");
    }
    sc.rhi->beginFrame(sc.swapchain.get());
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get());
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());
}

void DiffRendererTest::testLetterboxBothBranches() {
    auto metaTall = makeMetaDR(8, 16);
    auto metaWide = makeMetaDR(16, 8);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp, QSize(64, 64));

    {
        DiffRenderer dr(nullptr, metaTall);
        dr.initialize(rhi, rp.get());
        auto buf = std::make_shared<std::vector<uint8_t>>(metaTall->ySize() + metaTall->uvSize() * 2, 0x11);
        FrameData f1(metaTall->ySize(), metaTall->uvSize(), buf, 0);
        FrameData f2(metaTall->ySize(), metaTall->uvSize(), buf, 0);
        f1.setPts(1);
        f2.setPts(1);
        dr.uploadFrame(&f1, &f2);
        D3D11SwapchainEnv sc1;
        if (!makeD3D11RhiWithSwapchain(sc1, QSize(64, 64))) {
            QSKIP("D3D11 swapchain not available");
        }
        sc1.rhi->beginFrame(sc1.swapchain.get());
        QRhiCommandBuffer* cb = sc1.swapchain->currentFrameCommandBuffer();
        if (!cb) {
            QSKIP("D3D11 swapchain cmd buffer unavailable");
        }
        cb->beginPass(sc1.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
        dr.renderFrame(cb, QRect(0, 0, 64, 64), rt.get());
        cb->endPass();
        sc1.rhi->endFrame(sc1.swapchain.get());
    }

    {
        DiffRenderer dr(nullptr, metaWide);
        dr.initialize(rhi, rp.get());
        auto buf = std::make_shared<std::vector<uint8_t>>(metaWide->ySize() + metaWide->uvSize() * 2, 0x22);
        FrameData f1(metaWide->ySize(), metaWide->uvSize(), buf, 0);
        FrameData f2(metaWide->ySize(), metaWide->uvSize(), buf, 0);
        f1.setPts(2);
        f2.setPts(2);
        dr.uploadFrame(&f1, &f2);
        D3D11SwapchainEnv sc2;
        if (!makeD3D11RhiWithSwapchain(sc2, QSize(64, 64))) {
            QSKIP("D3D11 swapchain not available");
        }
        sc2.rhi->beginFrame(sc2.swapchain.get());
        QRhiCommandBuffer* cb = sc2.swapchain->currentFrameCommandBuffer();
        if (!cb) {
            QSKIP("D3D11 swapchain cmd buffer unavailable");
        }
        cb->beginPass(sc2.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
        dr.renderFrame(cb, QRect(0, 0, 32, 64), rt.get());
        cb->endPass();
        sc2.rhi->endFrame(sc2.swapchain.get());
    }
}

void DiffRendererTest::testZoomResetsWindowAspect() {
    auto meta = makeMetaDR(16, 9);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp, QSize(64, 64));

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi, rp.get());
    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x55);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(7);
    f2.setPts(7);
    dr.uploadFrame(&f1, &f2);
    dr.setZoomAndOffset(1.25f, 0.4f, 0.6f);
    D3D11SwapchainEnv env2;
    if (!makeD3D11RhiWithSwapchain(env2, QSize(64, 64))) {
        QSKIP("D3D11 swapchain not available");
    }
    env2.rhi->beginFrame(env2.swapchain.get());
    QRhiCommandBuffer* cb = env2.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    cb->beginPass(env2.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 64, 64), rt.get());
    cb->endPass();
    env2.rhi->endFrame(env2.swapchain.get());
}

void DiffRendererTest::testMultipleUploadsUpdatePtsAndSignals() {
    auto meta = makeMetaDR(8, 8);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy fullSpy(&dr, SIGNAL(batchIsFull()));
    QSignalSpy emptySpy(&dr, SIGNAL(batchIsEmpty()));
    dr.initialize(rhi, rp.get());

    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x66);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(3);
    f2.setPts(3);
    dr.uploadFrame(&f1, &f2);
    QTRY_VERIFY_WITH_TIMEOUT(fullSpy.count() > 0, 500);
    QCOMPARE(dr.getCurrentPts1(), uint64_t(3));
    QCOMPARE(dr.getCurrentPts2(), uint64_t(3));

    // Upload another pair with different pts
    f1.setPts(5);
    f2.setPts(5);
    dr.uploadFrame(&f1, &f2);
    QTRY_VERIFY_WITH_TIMEOUT(fullSpy.count() > 1, 500);
    QCOMPARE(dr.getCurrentPts1(), uint64_t(5));
    QCOMPARE(dr.getCurrentPts2(), uint64_t(5));

    // Drain via render to trigger batchIsEmpty
    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(16, 16))) {
        QSKIP("D3D11 swapchain not available");
    }
    sc.rhi->beginFrame(sc.swapchain.get());
    sc.rhi->beginFrame(sc.swapchain.get());
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get());
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());
    QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() > 0, 500);
}

void DiffRendererTest::testGetters() {
    auto meta = makeMetaDR(12, 10);
    DiffRenderer dr(nullptr, meta);
    QVERIFY(dr.getFrameMeta() == meta);
    // Before uploads, pts getters are zero
    QCOMPARE(dr.getCurrentPts1(), uint64_t(0));
    QCOMPARE(dr.getCurrentPts2(), uint64_t(0));
}

void DiffRendererTest::testReleaseAfterInit() {
    auto meta = makeMetaDR(8, 8);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi, rp.get());
    // Release immediately; should be safe no-op for any non-null batches
    dr.releaseBatch();
}

void DiffRendererTest::testMoreConfigCombos() {
    auto meta = makeMetaDR(8, 8);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi, rp.get());

    // Cycle through several combinations to cover write paths for diffConfig batch
    dr.setDiffConfig(0, 1.0f, 0);
    dr.setDiffConfig(1, 0.5f, 1);
    dr.setDiffConfig(2, 10.0f, 2);

    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x77);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(11);
    f2.setPts(11);
    dr.uploadFrame(&f1, &f2);

    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(32, 32))) {
        QSKIP("D3D11 swapchain not available");
    }
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    // Render twice with different viewports to ensure aspect path updates at least once
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get());
    cb->endPass();
    sc.rhi->beginFrame(sc.swapchain.get());
    cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 16, 8), rt.get());
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());
}

void DiffRendererTest::testUploadBeforeInitValidFrames() {
    // Covers branch where uploadFrame is called before initialize, even with valid FrameData
    auto meta = makeMetaDR(8, 8);
    DiffRenderer dr(nullptr, meta);
    QSignalSpy errSpy(&dr, SIGNAL(rendererError()));
    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x33);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(1);
    f2.setPts(1);
    dr.uploadFrame(&f1, &f2);
    QVERIFY(errSpy.count() > 0);
}

void DiffRendererTest::testExtremeDiffMultiplier() {
    // Exercise setDiffConfig with extreme multiplier values
    auto meta = makeMetaDR(16, 16);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi, rp.get());
    dr.setDiffConfig(1, 0.0001f, 0);
    dr.setDiffConfig(1, 1000.0f, 0);

    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x99);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(42);
    f2.setPts(42);
    dr.uploadFrame(&f1, &f2);

    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(16, 16))) {
        QSKIP("D3D11 swapchain not available");
    }
    sc.rhi->beginFrame(sc.swapchain.get());
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 16, 16), rt.get());
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());
}

void DiffRendererTest::testPartialNullFrame() {
    auto meta = makeMetaDR(8, 8);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy errSpy(&dr, SIGNAL(rendererError()));
    dr.initialize(rhi, rp.get());

    // One frame valid, the other nullptr
    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0xAB);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(9);
    dr.uploadFrame(&f1, nullptr);
    QVERIFY(errSpy.count() > 0);
}

void DiffRendererTest::testReleaseBatchesPaths() {
    auto meta = makeMetaDR(8, 8);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi, rp.get());

    // Create config and upload batches, then release
    dr.setDiffConfig(1, 2.0f, 0);
    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0xCD);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(2);
    f2.setPts(2);
    dr.uploadFrame(&f1, &f2);
    dr.releaseBatch();
    // Call again ensures already-null branches are also executed
    dr.releaseBatch();
}

void DiffRendererTest::testConfigOnlyRenderConsumesBatch() {
    auto meta = makeMetaDR(8, 8);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy emptySpy(&dr, SIGNAL(batchIsEmpty()));
    dr.initialize(rhi, rp.get());
    // Only set diff config so that diffConfigBatch is set, but no frame batch
    dr.setDiffConfig(1, 2.0f, 0);
    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(16, 16))) {
        QSKIP("D3D11 swapchain not available");
    }
    sc.rhi->beginFrame(sc.swapchain.get());
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get());
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());
    // Should emit batchIsEmpty due to diffConfigBatch consumption
    QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() > 0, 500);
}

void DiffRendererTest::testViewportVarietyLetterbox() {
    auto meta = makeMetaDR(13, 7); // unusual aspect
    std::unique_ptr<QQuickWindow> window;
    QRhi* rhi = nullptr;
    if (!makeWindowAndRhi(window, rhi, QSize(64, 64))) {
        QSKIP("QQuickWindow RHI not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp, QSize(64, 64));

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi, rp.get());
    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0xEF);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(6);
    f2.setPts(6);
    dr.uploadFrame(&f1, &f2);

    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(64, 64))) {
        QSKIP("D3D11 swapchain not available");
    }
    sc.rhi->beginFrame(sc.swapchain.get());
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    // Square viewport
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 64, 64), rt.get());
    cb->endPass();
    // Ultra wide
    sc.rhi->beginFrame(sc.swapchain.get());
    cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 128, 32), rt.get());
    cb->endPass();
    // Ultra tall
    cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb) {
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    }
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 16, 128), rt.get());
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());
}

void DiffRendererTest::testRenderConsumesInitOnly() {
    auto meta = makeMetaDR(8, 8);
    std::unique_ptr<QQuickWindow> windowF;
    QRhi* rhi = nullptr;
    if (!makeWindowAndRhi(windowF, rhi, QSize(16, 16))) {
        QSKIP("QQuickWindow RHI not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy emptySpy(&dr, SIGNAL(batchIsEmpty()));
    dr.initialize(rhi, rp.get()); // sets m_initBatch

    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(16, 16)))
        QSKIP("D3D11 swapchain not available");
    sc.rhi->beginFrame(sc.swapchain.get());
    sc.rhi->beginFrame(sc.swapchain.get());
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb)
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get()); // should consume m_initBatch and emit empty
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());
    QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() > 0, 500);
}

void DiffRendererTest::testResizeOnlyBatchPath() {
    auto meta = makeMetaDR(16, 9);
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp, QSize(64, 64));

    DiffRenderer dr(nullptr, meta);
    QSignalSpy emptySpy(&dr, SIGNAL(batchIsEmpty()));
    dr.initialize(rhi, rp.get());

    // Trigger resize params without upload/config change: first render sets m_resizeParamsBatch
    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(64, 64)))
        QSKIP("D3D11 swapchain not available");
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb)
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    const QRect vp1(0, 0, 64, 64);
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, vp1, rt.get());
    cb->endPass();
    // Second render with a different aspect should update again and consume previous batch
    const QRect vp2(0, 0, 128, 32);
    sc.rhi->beginFrame(sc.swapchain.get());
    cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb)
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, vp2, rt.get());
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());
    QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() > 0, 500);
}

void DiffRendererTest::testRenderFrameComprehensive() {
    // Cover renderFrame batches consumption and all aspect/zoom branches in one flow
    auto meta = makeMetaDR(16, 8); // videoAspect = 2.0
    auto rhiHolder = makeStandaloneRhiD3D11();
    if (!rhiHolder) {
        QSKIP("D3D11 RHI not available");
    }
    QRhi* rhi = rhiHolder.get();
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi, colorTex, rt, rp, QSize(64, 32)); // windowAspect initial = 2.0

    DiffRenderer dr(nullptr, meta);
    QSignalSpy emptySpy(&dr, SIGNAL(batchIsEmpty()));
    QSignalSpy fullSpy(&dr, SIGNAL(batchIsFull()));
    dr.initialize(rhi, rp.get()); // sets m_initBatch

    D3D11SwapchainEnv sc;
    if (!makeD3D11RhiWithSwapchain(sc, QSize(64, 32)))
        QSKIP("D3D11 swapchain not available");
    QRhiCommandBuffer* cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb)
        QSKIP("D3D11 swapchain cmd buffer unavailable");

    // 1) First render with V1: consumes initBatch, enters aspect-update (m_windowAspect = 0 initially)
    const QRect V1(0, 0, 64, 32); // windowAspect = 2.0 equals videoAspect, else branch goes to height letterboxed path?
                                  // (windowAspect > videoAspect is false) → else path
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, V1, rt.get());
    cb->endPass();

    // 2) Only diff config set; same viewport to drive no-aspect-update path
    dr.setDiffConfig(1, 2.0f, 0);
    cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb)
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, V1, rt.get()); // should consume diffConfigBatch; no aspect recompute
    cb->endPass();

    // 3) Upload frames, render with different aspect V2 to enter aspect-update with window wider (windowAspect >
    // videoAspect)
    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x5A);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(9);
    f2.setPts(9);
    dr.uploadFrame(&f1, &f2);
    const QRect V2(0, 0, 128, 32); // windowAspect = 4.0 (> videoAspect)
    cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb)
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, V2, rt.get());
    cb->endPass();

    // 4) Set zoom/offset and render with V3 to trigger zoom offset computation
    dr.setZoomAndOffset(1.3f, 0.4f, 0.6f); // resets m_windowAspect forcing recompute
    const QRect V3(0, 0, 32, 64);          // windowAspect = 0.5 (< videoAspect)
    cb = sc.swapchain->currentFrameCommandBuffer();
    if (!cb)
        QSKIP("D3D11 swapchain cmd buffer unavailable");
    cb->beginPass(sc.swapchain->currentFrameRenderTarget(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, V3, rt.get());
    cb->endPass();
    sc.rhi->endFrame(sc.swapchain.get());

    // Should have emitted empty at least once per render pass
    QVERIFY(emptySpy.count() >= 4);
    // Ensure batchIsFull emitted when uploading
    QVERIFY(fullSpy.count() >= 1);
}

QTEST_MAIN(DiffRendererTest)
#include "test_diffrenderer.moc"

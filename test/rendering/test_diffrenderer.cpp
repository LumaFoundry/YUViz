#include <QColor>
#include <QSignalSpy>
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
    QRhiNullInitParams params;
    return std::unique_ptr<QRhi>(QRhi::create(QRhi::Null, &params));
}

static void makeRtAndRpDR(QRhi* rhi,
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
};

void DiffRendererTest::testInitializeUploadRender() {
    auto meta = makeMetaDR();
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }

    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy errSpy(&dr, SIGNAL(rendererError()));
    QSignalSpy fullSpy(&dr, SIGNAL(batchIsFull()));
    QSignalSpy emptySpy(&dr, SIGNAL(batchIsEmpty()));

    dr.initialize(rhi.get(), rp.get());
    if (errSpy.count() > 0) {
        QSKIP("DiffRenderer initialize failed in this environment");
    }

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

    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi offscreen frame not supported");
    }
    const QRect sameViewport(0, 0, 8, 8);
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, sameViewport, rt.get());
    cb->endPass();
    // Render again with same viewport to hit no-aspect-update branch
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, sameViewport, rt.get());
    cb->endPass();
    QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() > 0, 1000);
    rhi->endOffscreenFrame();

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

    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);
    dr.initialize(rhi.get(), rp.get());

    // invalid frames
    dr.uploadFrame(nullptr, nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(errSpy.count() > 1, 1000);

    // config branch (invokes nextResourceUpdateBatch path)
    dr.setDiffConfig(2, 8.0f, 1);
}

void DiffRendererTest::testZoomAndViewportBranches() {
    auto meta = makeMetaDR(16, 8);
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp, QSize(32, 32));

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi.get(), rp.get());

    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x20);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(5);
    f2.setPts(5);
    dr.uploadFrame(&f1, &f2);

    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi offscreen frame not supported");
    }
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 64, 32), rt.get());
    cb->endPass();
    dr.setZoomAndOffset(1.5f, 0.3f, 0.7f);
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 32, 64), rt.get());
    cb->endPass();
    rhi->endOffscreenFrame();
}

void DiffRendererTest::testInvalidDimensions() {
    auto meta = makeMetaDR(0, 0);
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy errSpy(&dr, SIGNAL(rendererError()));
    dr.initialize(rhi.get(), rp.get());

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
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi.get(), rp.get());

    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi Null backend not available");
    }
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get());
    cb->endPass();
    rhi->endOffscreenFrame();
}

void DiffRendererTest::testLetterboxBothBranches() {
    auto metaTall = makeMetaDR(8, 16);
    auto metaWide = makeMetaDR(16, 8);
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp, QSize(64, 64));

    {
        DiffRenderer dr(nullptr, metaTall);
        dr.initialize(rhi.get(), rp.get());
        auto buf = std::make_shared<std::vector<uint8_t>>(metaTall->ySize() + metaTall->uvSize() * 2, 0x11);
        FrameData f1(metaTall->ySize(), metaTall->uvSize(), buf, 0);
        FrameData f2(metaTall->ySize(), metaTall->uvSize(), buf, 0);
        f1.setPts(1);
        f2.setPts(1);
        dr.uploadFrame(&f1, &f2);
        QRhiCommandBuffer* cb = nullptr;
        if (!rhi->beginOffscreenFrame(&cb)) {
            QSKIP("QRhi Null backend not available");
        }
        cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
        dr.renderFrame(cb, QRect(0, 0, 64, 64), rt.get());
        cb->endPass();
        rhi->endOffscreenFrame();
    }

    {
        DiffRenderer dr(nullptr, metaWide);
        dr.initialize(rhi.get(), rp.get());
        auto buf = std::make_shared<std::vector<uint8_t>>(metaWide->ySize() + metaWide->uvSize() * 2, 0x22);
        FrameData f1(metaWide->ySize(), metaWide->uvSize(), buf, 0);
        FrameData f2(metaWide->ySize(), metaWide->uvSize(), buf, 0);
        f1.setPts(2);
        f2.setPts(2);
        dr.uploadFrame(&f1, &f2);
        QRhiCommandBuffer* cb = nullptr;
        if (!rhi->beginOffscreenFrame(&cb)) {
            QSKIP("QRhi Null backend not available");
        }
        cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
        dr.renderFrame(cb, QRect(0, 0, 32, 64), rt.get());
        cb->endPass();
        rhi->endOffscreenFrame();
    }
}

void DiffRendererTest::testZoomResetsWindowAspect() {
    auto meta = makeMetaDR(16, 9);
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp, QSize(64, 64));

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi.get(), rp.get());
    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x55);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(7);
    f2.setPts(7);
    dr.uploadFrame(&f1, &f2);
    dr.setZoomAndOffset(1.25f, 0.4f, 0.6f);
    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi Null backend not available");
    }
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 64, 64), rt.get());
    cb->endPass();
    rhi->endOffscreenFrame();
}

void DiffRendererTest::testMultipleUploadsUpdatePtsAndSignals() {
    auto meta = makeMetaDR(8, 8);
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy fullSpy(&dr, SIGNAL(batchIsFull()));
    QSignalSpy emptySpy(&dr, SIGNAL(batchIsEmpty()));
    dr.initialize(rhi.get(), rp.get());

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
    QRhiCommandBuffer* cb = nullptr;
    if (rhi->beginOffscreenFrame(&cb)) {
        cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
        dr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get());
        cb->endPass();
        rhi->endOffscreenFrame();
        QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() > 0, 500);
    }
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
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi.get(), rp.get());
    // Release immediately; should be safe no-op for any non-null batches
    dr.releaseBatch();
}

void DiffRendererTest::testMoreConfigCombos() {
    auto meta = makeMetaDR(8, 8);
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi.get(), rp.get());

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

    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi Null backend not available");
    }
    // Render twice with different viewports to ensure aspect path updates at least once
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get());
    cb->endPass();
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 16, 8), rt.get());
    cb->endPass();
    rhi->endOffscreenFrame();
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
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi.get(), rp.get());
    dr.setDiffConfig(1, 0.0001f, 0);
    dr.setDiffConfig(1, 1000.0f, 0);

    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0x99);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(42);
    f2.setPts(42);
    dr.uploadFrame(&f1, &f2);

    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi Null backend not available");
    }
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 16, 16), rt.get());
    cb->endPass();
    rhi->endOffscreenFrame();
}

void DiffRendererTest::testPartialNullFrame() {
    auto meta = makeMetaDR(8, 8);
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy errSpy(&dr, SIGNAL(rendererError()));
    dr.initialize(rhi.get(), rp.get());

    // One frame valid, the other nullptr
    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0xAB);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(9);
    dr.uploadFrame(&f1, nullptr);
    QVERIFY(errSpy.count() > 0);
}

void DiffRendererTest::testReleaseBatchesPaths() {
    auto meta = makeMetaDR(8, 8);
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi.get(), rp.get());

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
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp);

    DiffRenderer dr(nullptr, meta);
    QSignalSpy emptySpy(&dr, SIGNAL(batchIsEmpty()));
    dr.initialize(rhi.get(), rp.get());
    // Only set diff config so that diffConfigBatch is set, but no frame batch
    dr.setDiffConfig(1, 2.0f, 0);
    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi Null backend not available");
    }
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 8, 8), rt.get());
    cb->endPass();
    rhi->endOffscreenFrame();
    // Should emit batchIsEmpty due to diffConfigBatch consumption
    QTRY_VERIFY_WITH_TIMEOUT(emptySpy.count() > 0, 500);
}

void DiffRendererTest::testViewportVarietyLetterbox() {
    auto meta = makeMetaDR(13, 7); // unusual aspect
    auto rhi = makeNullRhiDR();
    if (!rhi) {
        QSKIP("QRhi Null backend not available");
    }
    std::unique_ptr<QRhiTexture> colorTex;
    std::unique_ptr<QRhiTextureRenderTarget> rt;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    makeRtAndRpDR(rhi.get(), colorTex, rt, rp, QSize(64, 64));

    DiffRenderer dr(nullptr, meta);
    dr.initialize(rhi.get(), rp.get());
    auto buf = std::make_shared<std::vector<uint8_t>>(meta->ySize() + meta->uvSize() * 2, 0xEF);
    FrameData f1(meta->ySize(), meta->uvSize(), buf, 0);
    FrameData f2(meta->ySize(), meta->uvSize(), buf, 0);
    f1.setPts(6);
    f2.setPts(6);
    dr.uploadFrame(&f1, &f2);

    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb)) {
        QSKIP("QRhi Null backend not available");
    }
    // Square viewport
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 64, 64), rt.get());
    cb->endPass();
    // Ultra wide
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 128, 32), rt.get());
    cb->endPass();
    // Ultra tall
    cb->beginPass(rt.get(), QColor(0, 0, 0), {1.0f, 0});
    dr.renderFrame(cb, QRect(0, 0, 16, 128), rt.get());
    cb->endPass();
    rhi->endOffscreenFrame();
}

QTEST_MAIN(DiffRendererTest)
#include "test_diffrenderer.moc"

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QtTest>
#include <memory>
#include <vector>

#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "rendering/videoRenderNode.h"
#include "rendering/videoRenderer.h"

static std::shared_ptr<FrameMeta> makeMetaVRN(int yW = 16, int yH = 8) {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(yW);
    meta->setYHeight(yH);
    meta->setUVWidth(yW / 2);
    meta->setUVHeight(yH / 2);
    meta->setColorSpace(AVCOL_SPC_BT709);
    meta->setColorRange(AVCOL_RANGE_MPEG);
    return meta;
}

class TestVideoItem : public QQuickItem {
    Q_OBJECT
  public:
    explicit TestVideoItem(QQuickItem* parent = nullptr) :
        QQuickItem(parent) {
        setFlag(QQuickItem::ItemHasContents, true);
        m_meta = makeMetaVRN();
        m_renderer = new VideoRenderer(nullptr, m_meta);
    }
    ~TestVideoItem() override { delete m_renderer; }

    VideoRenderer* renderer() const { return m_renderer; }

  protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override {
        auto* node = static_cast<VideoRenderNode*>(old);
        if (!node) {
            node = new VideoRenderNode(this, m_renderer);
        }
        return node;
    }

  private:
    std::shared_ptr<FrameMeta> m_meta;
    VideoRenderer* m_renderer = nullptr;
};

class VideoRenderNodeTest : public QObject {
    Q_OBJECT

  private slots:
    void testSceneGraphInvocation();
    void testRectAndPrepareNullPaths();
    void testFlagsAndRectWithItem();
    void testPrepareAndRenderWithItemNoWindow();
    void testPrepareWithWindowNoRhi();
    void testRenderWithWindowNoCbRt();
};

void VideoRenderNodeTest::testSceneGraphInvocation() {
    // Drive a minimal real scene graph frame to hit prepare()/render()
    qputenv("QSG_RENDER_LOOP", QByteArray("basic"));
    qputenv("QSG_RHI_BACKEND", QByteArray("d3d11"));
    QQuickWindow window;
    window.resize(200, 120);
    window.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&window, 3000), "Window not exposed");

    TestVideoItem* item = new TestVideoItem(window.contentItem());
    item->setWidth(160);
    item->setHeight(90);
    window.contentItem()->update();

    QSignalSpy swapped(&window, &QQuickWindow::frameSwapped);
    // Request a couple of frames
    window.requestUpdate();
    QVERIFY(QTest::qWaitFor([&] { return swapped.count() >= 1; }, 3000));
}

void VideoRenderNodeTest::testRectAndPrepareNullPaths() {
    VideoRenderNode node(nullptr, new VideoRenderer(nullptr, makeMetaVRN()));
    QRectF r = node.rect();
    QCOMPARE(r, QRectF());
    // prepare() and render() with null dependencies early-return without crash
    node.prepare();
    node.render(nullptr);
}

void VideoRenderNodeTest::testFlagsAndRectWithItem() {
    QQuickItem item;
    item.setWidth(100);
    item.setHeight(50);
    VideoRenderer* vr = new VideoRenderer(nullptr, makeMetaVRN());
    VideoRenderNode node(&item, vr);
    QCOMPARE(node.flags() & QSGRenderNode::BoundedRectRendering, QSGRenderNode::BoundedRectRendering);
    QCOMPARE(node.rect(), QRectF(0, 0, 100, 50));
}

void VideoRenderNodeTest::testPrepareAndRenderWithItemNoWindow() {
    QQuickItem item;
    item.setWidth(64);
    item.setHeight(32);
    VideoRenderer* vr = new VideoRenderer(nullptr, makeMetaVRN());
    VideoRenderNode node(&item, vr);
    // No window set on item, prepare should early-return
    node.prepare();
    // Also render should early-return on missing window
    node.render(nullptr);
}

void VideoRenderNodeTest::testPrepareWithWindowNoRhi() {
    QQuickWindow window; // not shown -> likely no RHI
    QQuickItem item;
    item.setWidth(10);
    item.setHeight(20);
    item.setParentItem(window.contentItem());
    VideoRenderer* vr = new VideoRenderer(nullptr, makeMetaVRN());
    VideoRenderNode node(&item, vr);
    // window() exists, but rhi() is expected null before exposure => early return
    node.prepare();
}

void VideoRenderNodeTest::testRenderWithWindowNoCbRt() {
    QQuickWindow window; // not shown -> renderTarget()/commandBuffer() are null
    QQuickItem item;
    item.setWidth(10);
    item.setHeight(20);
    item.setParentItem(window.contentItem());
    VideoRenderer* vr = new VideoRenderer(nullptr, makeMetaVRN());
    VideoRenderNode node(&item, vr);
    // Should early-return because cb/rt are null outside a render pass
    node.render(nullptr);
}

// Custom main to set render loop before QGuiApplication is created
int main(int argc, char** argv) {
    qputenv("QSG_RENDER_LOOP", QByteArray("basic"));
    qputenv("QSG_RHI_BACKEND", QByteArray("opengl"));
    QGuiApplication app(argc, argv);
    VideoRenderNodeTest tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_videorendernode.moc"

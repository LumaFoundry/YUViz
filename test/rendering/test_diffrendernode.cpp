#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QtTest>
#include <memory>
#include <vector>

#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "rendering/diffRenderNode.h"
#include "rendering/diffRenderer.h"

static std::shared_ptr<FrameMeta> makeMetaDRN(int yW = 16, int yH = 8) {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(yW);
    meta->setYHeight(yH);
    meta->setUVWidth(yW / 2);
    meta->setUVHeight(yH / 2);
    return meta;
}

class TestDiffItem : public QQuickItem {
    Q_OBJECT
  public:
    explicit TestDiffItem(QQuickItem* parent = nullptr) :
        QQuickItem(parent) {
        setFlag(QQuickItem::ItemHasContents, true);
        m_meta = makeMetaDRN();
        m_renderer = new DiffRenderer(nullptr, m_meta);
    }
    ~TestDiffItem() override { delete m_renderer; }
    DiffRenderer* renderer() const { return m_renderer; }

  protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override {
        auto* node = static_cast<DiffRenderNode*>(old);
        if (!node) {
            node = new DiffRenderNode(this, m_renderer);
        }
        return node;
    }

  private:
    std::shared_ptr<FrameMeta> m_meta;
    DiffRenderer* m_renderer = nullptr;
};

class DiffRenderNodeTest : public QObject {
    Q_OBJECT
  private slots:
    void testSceneGraphInvocation();
    void testRectNull();
    void testFlagsAndRectWithItem();
    void testPrepareAndRenderWithItemNoWindow();
    void testPrepareWithWindowNoRhi();
    void testRenderWithWindowNoCbRt();
};

void DiffRenderNodeTest::testSceneGraphInvocation() {
    // Drive a minimal real scene graph frame to hit prepare()/render()
    qputenv("QSG_RENDER_LOOP", QByteArray("basic"));
    qputenv("QSG_RHI_BACKEND", QByteArray("d3d11"));
    QQuickWindow window;
    window.resize(200, 120);
    window.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&window, 3000), "Window not exposed");

    TestDiffItem* item = new TestDiffItem(window.contentItem());
    item->setWidth(160);
    item->setHeight(90);
    window.contentItem()->update();

    QSignalSpy swapped(&window, &QQuickWindow::frameSwapped);
    window.requestUpdate();
    QVERIFY(QTest::qWaitFor([&] { return swapped.count() >= 1; }, 3000));
}

void DiffRenderNodeTest::testRectNull() {
    DiffRenderNode node(nullptr, new DiffRenderer(nullptr, makeMetaDRN()));
    QRectF r = node.rect();
    QCOMPARE(r, QRectF());
    node.prepare();
    node.render(nullptr);
}

void DiffRenderNodeTest::testFlagsAndRectWithItem() {
    QQuickItem item;
    item.setWidth(120);
    item.setHeight(60);
    DiffRenderer* dr = new DiffRenderer(nullptr, makeMetaDRN());
    DiffRenderNode node(&item, dr);
    QCOMPARE(node.flags() & QSGRenderNode::BoundedRectRendering, QSGRenderNode::BoundedRectRendering);
    QCOMPARE(node.rect(), QRectF(0, 0, 120, 60));
}

void DiffRenderNodeTest::testPrepareAndRenderWithItemNoWindow() {
    QQuickItem item;
    item.setWidth(40);
    item.setHeight(20);
    DiffRenderer* dr = new DiffRenderer(nullptr, makeMetaDRN());
    DiffRenderNode node(&item, dr);
    node.prepare();
    node.render(nullptr);
}

void DiffRenderNodeTest::testPrepareWithWindowNoRhi() {
    QQuickWindow window; // not exposed => no RHI
    QQuickItem item;
    item.setWidth(10);
    item.setHeight(20);
    item.setParentItem(window.contentItem());
    DiffRenderer* dr = new DiffRenderer(nullptr, makeMetaDRN());
    DiffRenderNode node(&item, dr);
    node.prepare(); // early-return at !rhi
}

void DiffRenderNodeTest::testRenderWithWindowNoCbRt() {
    QQuickWindow window; // not in a render pass => no cb/rt
    QQuickItem item;
    item.setWidth(10);
    item.setHeight(20);
    item.setParentItem(window.contentItem());
    DiffRenderer* dr = new DiffRenderer(nullptr, makeMetaDRN());
    DiffRenderNode node(&item, dr);
    node.render(nullptr); // early-return at !cb || !rt
}

int main(int argc, char** argv) {
    qputenv("QSG_RENDER_LOOP", QByteArray("basic"));
    qputenv("QSG_RHI_BACKEND", QByteArray("opengl"));
    QGuiApplication app(argc, argv);
    DiffRenderNodeTest tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_diffrendernode.moc"

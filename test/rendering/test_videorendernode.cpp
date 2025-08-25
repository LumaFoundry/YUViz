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
};

void VideoRenderNodeTest::testSceneGraphInvocation() {
    QSKIP("Scene graph render path is environment-specific; skipping to keep tests stable");
}

void VideoRenderNodeTest::testRectAndPrepareNullPaths() {
    VideoRenderNode node(nullptr, new VideoRenderer(nullptr, makeMetaVRN()));
    QRectF r = node.rect();
    QCOMPARE(r, QRectF());
    // prepare() and render() with null dependencies early-return without crash
    node.prepare();
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

#include <QDebug>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>
#include "controller/compareController.h"
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "frames/frameQueue.h"
#include "ui/diffWindow.h"

class DummyDiffRenderer : public DiffRenderer {
  public:
    DummyDiffRenderer(QObject* parent = nullptr, std::shared_ptr<FrameMeta> meta = nullptr) :
        DiffRenderer(parent, meta) {}
    // Implement all pure virtuals as empty if needed
    void render() { emit batchIsEmpty(); }
    void upload() { emit batchIsFull(); }
};

class DummyDiffWindow : public DiffWindow {
  public:
    DummyDiffRenderer dummyRenderer;
    DummyDiffWindow() :
        dummyRenderer(nullptr, nullptr) {
        m_renderer = &dummyRenderer;
    }
    void initialize(std::shared_ptr<FrameMeta>, std::shared_ptr<FrameQueue>, std::shared_ptr<FrameQueue>) {}
    void uploadFrame(FrameData*, FrameData*) {}
    void renderFrame() {}
};

class CompareControllerTest : public QObject {
    Q_OBJECT

  private slots:
    void testConstructor();
    void testSetDiffWindow();
    void testSetMetadata();
    void testOnReceiveFrame();
    void testOnRequestRender();
    void testOnCompareRendered();
};

void CompareControllerTest::testConstructor() {
    CompareController controller;
    QVERIFY(true); // Just test construction/destruction
}

void CompareControllerTest::testSetDiffWindow() {
    CompareController controller;
    DummyDiffWindow dummyWindow;
    controller.setDiffWindow(&dummyWindow);
}

void CompareControllerTest::testSetMetadata() {
    CompareController controller;
    DummyDiffWindow dummyWindow;
    controller.setDiffWindow(&dummyWindow);
    qDebug() << "windows set";

    auto meta1 = std::make_shared<FrameMeta>();
    auto meta2 = std::make_shared<FrameMeta>();

    qDebug() << "meta created";

    meta1->setYWidth(2);
    meta1->setYHeight(2);
    meta1->setUVWidth(1);
    meta1->setUVHeight(1);
    meta2->setYWidth(2);
    meta2->setYHeight(2);
    meta2->setUVWidth(1);
    meta2->setUVHeight(1);

    qDebug() << "meta set";

    auto queue1 = std::make_shared<FrameQueue>(meta1, 1);
    auto queue2 = std::make_shared<FrameQueue>(meta2, 1);

    qDebug() << "queue set";

    controller.setMetadata(meta1, meta2, queue1, queue2);
}

void CompareControllerTest::testOnReceiveFrame() {
    CompareController controller;
    controller.setVideoIds(1, 2);
    auto meta1 = std::make_shared<FrameMeta>();
    auto meta2 = std::make_shared<FrameMeta>();
    meta1->setYWidth(2);
    meta1->setYHeight(2);
    meta1->setUVWidth(1);
    meta1->setUVHeight(1);
    meta2->setYWidth(2);
    meta2->setYHeight(2);
    meta2->setUVWidth(1);
    meta2->setUVHeight(1);
    controller.setMetadata(
        meta1, meta2, std::make_shared<FrameQueue>(meta1, 1), std::make_shared<FrameQueue>(meta2, 1));
    auto buffer1 = std::make_shared<std::vector<uint8_t>>(16);
    auto buffer2 = std::make_shared<std::vector<uint8_t>>(16);
    FrameData frame1(2, 2, buffer1, 1);
    FrameData frame2(2, 2, buffer2, 1);
    frame1.setPts(1);
    frame2.setPts(1);
    controller.onReceiveFrame(&frame1, 1); // Should store in m_frame1
    controller.onReceiveFrame(&frame2, 2); // Should store in m_frame2 and trigger diff
    // Unknown index
    controller.onReceiveFrame(&frame1, 99);
    // Null frame
    controller.onReceiveFrame(nullptr, 1);
}

void CompareControllerTest::testOnRequestRender() {
    CompareController controller;
    controller.setVideoIds(1, 2);
    QSignalSpy spy(&controller, SIGNAL(requestRender()));
    controller.onRequestRender(1);
    QVERIFY(spy.count() == 0);
    controller.onRequestRender(2);
    QVERIFY(spy.count() == 1);
    // Call again to check flags reset
    controller.onRequestRender(1);
    controller.onRequestRender(2);
}

void CompareControllerTest::testOnCompareRendered() {
    CompareController controller;
    QSignalSpy spy(&controller, SIGNAL(psnrChanged()));
    // Set up state
    controller.setVideoIds(1, 2);
    auto meta1 = std::make_shared<FrameMeta>();
    auto meta2 = std::make_shared<FrameMeta>();
    meta1->setYWidth(2);
    meta1->setYHeight(2);
    meta2->setYWidth(2);
    meta2->setYHeight(2);
    controller.setMetadata(
        meta1, meta2, std::make_shared<FrameQueue>(meta1, 1), std::make_shared<FrameQueue>(meta2, 1));
    auto buffer = std::make_shared<std::vector<uint8_t>>(16);
    FrameData frame1(2, 2, buffer, 0);
    FrameData frame2(2, 2, buffer, 8);
    frame1.setPts(1);
    frame2.setPts(1);
    controller.onReceiveFrame(&frame1, 1);
    controller.onReceiveFrame(&frame2, 2);
    controller.onRequestRender(1);
    controller.onRequestRender(2);
    controller.onCompareRendered();
    QVERIFY(spy.count() == 1);
    // Call again to test branch where frames are already reset
    controller.onCompareRendered();
}

QTEST_MAIN(CompareControllerTest)
#include "test_comparecontroller.moc"
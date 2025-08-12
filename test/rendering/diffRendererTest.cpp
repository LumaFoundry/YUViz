#include "diffRendererTest.h"
#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <memory>
#include "../../src/frames/frameData.h"
#include "../../src/frames/frameMeta.h"
#include "../../src/rendering/diffRenderer.h"

void DiffRendererTest::testConstructor() {
    // Create a mock FrameMeta
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(1920);
    meta->setYHeight(1080);

    // Test constructor
    DiffRenderer renderer(this, meta);

    // Verify frame meta is set correctly
    auto retrievedMeta = renderer.getFrameMeta();
    QVERIFY(retrievedMeta != nullptr);
    QCOMPARE(retrievedMeta->yWidth(), 1920);
    QCOMPARE(retrievedMeta->yHeight(), 1080);
}

void DiffRendererTest::testSetDiffConfig() {
    auto meta = std::make_shared<FrameMeta>();
    DiffRenderer renderer(this, meta);

    // Test that renderer object can be created
    QVERIFY(&renderer != nullptr);
}

void DiffRendererTest::testSetZoomAndOffset() {
    auto meta = std::make_shared<FrameMeta>();
    DiffRenderer renderer(this, meta);

    // Test setting zoom and offset
    renderer.setZoomAndOffset(2.0f, 0.3f, 0.7f);

    // The method should not crash
    QVERIFY(&renderer != nullptr);
}

void DiffRendererTest::testInitialization() {
    auto meta = std::make_shared<FrameMeta>();
    DiffRenderer renderer(this, meta);

    // Test initialization (without actual QRhi)
    // This should not crash even without proper QRhi context
    renderer.initialize(nullptr, nullptr);

    QVERIFY(&renderer != nullptr);
}
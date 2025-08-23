#include <QtTest>
#include "utils/sharedViewProperties.h"

class SharedViewPropertiesTest : public QObject {
    Q_OBJECT
  private slots:
    void testInitialValues();
    void testSetZoom();
    void testSetCenterX();
    void testSetCenterY();
    void testReset();
    void testApplyPan();
    void testApplyZoom();
    void testZoomToSelection();
    void testViewChangedSignal();
};

void SharedViewPropertiesTest::testInitialValues() {
    SharedViewProperties props;
    QCOMPARE(props.zoom(), 1.0);
    QCOMPARE(props.centerX(), 0.5);
    QCOMPARE(props.centerY(), 0.5);
    QCOMPARE(props.isZoomed(), false);
}

void SharedViewPropertiesTest::testSetZoom() {
    SharedViewProperties props;
    QSignalSpy spy(&props, SIGNAL(viewChanged()));
    props.setZoom(2.0);
    QCOMPARE(props.zoom(), 2.0);
    QVERIFY(props.isZoomed());
    QCOMPARE(spy.count(), 1);
    props.setZoom(2.0); // No change
    QCOMPARE(spy.count(), 1);
    props.setZoom(0.5); // Should clamp to 1.0
    QCOMPARE(props.zoom(), 1.0);
}

void SharedViewPropertiesTest::testSetCenterX() {
    SharedViewProperties props;
    QSignalSpy spy(&props, SIGNAL(viewChanged()));
    props.setCenterX(0.7);
    QCOMPARE(props.centerX(), 0.7);
    QCOMPARE(spy.count(), 1);
    props.setCenterX(0.7); // No change
    QCOMPARE(spy.count(), 1);
}

void SharedViewPropertiesTest::testSetCenterY() {
    SharedViewProperties props;
    QSignalSpy spy(&props, SIGNAL(viewChanged()));
    props.setCenterY(0.3);
    QCOMPARE(props.centerY(), 0.3);
    QCOMPARE(spy.count(), 1);
    props.setCenterY(0.3); // No change
    QCOMPARE(spy.count(), 1);
}

void SharedViewPropertiesTest::testReset() {
    SharedViewProperties props;
    props.setZoom(3.0);
    props.setCenterX(0.8);
    props.setCenterY(0.2);
    QSignalSpy spy(&props, SIGNAL(viewChanged()));
    props.reset();
    QCOMPARE(props.zoom(), 1.0);
    QCOMPARE(props.centerX(), 0.5);
    QCOMPARE(props.centerY(), 0.5);
    QCOMPARE(spy.count(), 1);
}

void SharedViewPropertiesTest::testApplyPan() {
    SharedViewProperties props;
    props.setZoom(2.0);
    QSignalSpy spy(&props, SIGNAL(viewChanged()));
    props.applyPan(0.2, -0.2);
    QCOMPARE(props.centerX(), 0.5 + 0.2 / 2.0);
    QCOMPARE(props.centerY(), 0.5 - 0.2 / 2.0);
    QCOMPARE(spy.count(), 1);
    props.setZoom(1.0);
    spy.clear();
    props.applyPan(1.0, 1.0); // Should not change
    QCOMPARE(spy.count(), 0);
}

void SharedViewPropertiesTest::testApplyZoom() {
    SharedViewProperties props;
    QSignalSpy spy(&props, SIGNAL(viewChanged()));
    props.applyZoom(2.0, 0.5, 0.5); // Center zoom
    QCOMPARE(props.zoom(), 2.0);
    QCOMPARE(spy.count(), 1);
    props.applyZoom(0.5, 0.5, 0.5); // Back to 1.0, triggers reset
    QCOMPARE(props.zoom(), 1.0);
    QCOMPARE(props.centerX(), 0.5);
    QCOMPARE(props.centerY(), 0.5);
}

void SharedViewPropertiesTest::testZoomToSelection() {
    SharedViewProperties props;
    QSignalSpy spy(&props, SIGNAL(viewChanged()));
    QRectF selection(0.25, 0.25, 0.5, 0.5);
    QRectF videoRect(0.0, 0.0, 1.0, 1.0);
    props.zoomToSelection(selection, videoRect, 1.0, 0.5, 0.5);
    QVERIFY(props.zoom() > 1.0);
    QCOMPARE(spy.count(), 1);
}

void SharedViewPropertiesTest::testViewChangedSignal() {
    SharedViewProperties props;
    QSignalSpy spy(&props, SIGNAL(viewChanged()));
    props.setZoom(2.0);
    props.setCenterX(0.6);
    props.setCenterY(0.4);
    props.reset();
    QCOMPARE(spy.count(), 4);
}

QTEST_MAIN(SharedViewPropertiesTest)
#include "test_sharedviewproperties.moc"

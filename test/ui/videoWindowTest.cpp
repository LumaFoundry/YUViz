#include "videoWindowTest.h"
#include <QDebug>
#include <QQuickView>
#include <QSignalSpy>
#include <QTest>
#include <memory>
#include "../../src/frames/frameMeta.h"
#include "../../src/ui/videoWindow.h"
#include "../../src/utils/sharedViewProperties.h"

void VideoWindowTest::testConstructor() {
    // Test VideoWindow constructor
    VideoWindow window;

    // Test default property values
    QCOMPARE(window.getAspectRatio(), 0.0);
    QCOMPARE(window.maxZoom(), 10.0); // Default max zoom
    QCOMPARE(window.osdState(), 0);
    QCOMPARE(window.currentFrame(), 0);
    QCOMPARE(window.componentDisplayMode(), 0);

    // Test default string properties
    QVERIFY(window.pixelFormat().isEmpty());
    QVERIFY(window.timeBase().isEmpty());
    QCOMPARE(window.duration(), 0);
    QCOMPARE(window.currentTimeMs(), 0.0);
    QVERIFY(window.colorSpace().isEmpty());
    QVERIFY(window.colorRange().isEmpty());
    QVERIFY(window.videoResolution().isEmpty());
    QVERIFY(window.codecName().isEmpty());
    QVERIFY(window.videoName().isEmpty());
}

void VideoWindowTest::testWindowOperations() {
    VideoWindow window;

    // Test aspect ratio setting
    window.setAspectRatio(1920, 1080);
    QCOMPARE(window.getAspectRatio(), 16.0 / 9.0);

    // Test max zoom setting
    window.setMaxZoom(5.0);
    QCOMPARE(window.maxZoom(), 5.0);

    // Test component display mode
    window.setComponentDisplayMode(1);
    QCOMPARE(window.componentDisplayMode(), 1);

    // Test OSD state
    window.setOsdState(1);
    QCOMPARE(window.osdState(), 1);

    // Test zoom operations
    window.zoomAt(2.0, QPointF(0.5, 0.5));
    window.resetView();

    // Test selection operations
    window.setSelectionRect(QRectF(0.1, 0.1, 0.8, 0.8));
    window.clearSelection();
    window.zoomToSelection(QRectF(0.2, 0.2, 0.6, 0.6));

    // Test pan operation
    window.pan(QPointF(0.1, 0.1));

    // Test frame info update
    window.updateFrameInfo(100, 4000.0);

    // Test toggle OSD
    window.toggleOsd();
}

void VideoWindowTest::testErrorHandling() {
    VideoWindow window;

    // Test signal connections
    QSignalSpy batchUploadedSpy(&window, &VideoWindow::batchUploaded);
    QSignalSpy gpuUploadedSpy(&window, &VideoWindow::gpuUploaded);
    QSignalSpy errorOccurredSpy(&window, &VideoWindow::errorOccurred);
    QSignalSpy selectionChangedSpy(&window, &VideoWindow::selectionChanged);
    QSignalSpy zoomChangedSpy(&window, &VideoWindow::zoomChanged);
    QSignalSpy maxZoomChangedSpy(&window, &VideoWindow::maxZoomChanged);
    QSignalSpy sharedViewChangedSpy(&window, &VideoWindow::sharedViewChanged);
    QSignalSpy frameReadySpy(&window, &VideoWindow::frameReady);
    QSignalSpy osdStateChangedSpy(&window, &VideoWindow::osdStateChanged);
    QSignalSpy currentFrameChangedSpy(&window, &VideoWindow::currentFrameChanged);
    QSignalSpy currentTimeMsChangedSpy(&window, &VideoWindow::currentTimeMsChanged);
    QSignalSpy metadataInitializedSpy(&window, &VideoWindow::metadataInitialized);
    QSignalSpy componentDisplayModeChangedSpy(&window, &VideoWindow::componentDisplayModeChanged);

    // Verify signals are connected
    QVERIFY(batchUploadedSpy.isValid());
    QVERIFY(gpuUploadedSpy.isValid());
    QVERIFY(errorOccurredSpy.isValid());
    QVERIFY(selectionChangedSpy.isValid());
    QVERIFY(zoomChangedSpy.isValid());
    QVERIFY(maxZoomChangedSpy.isValid());
    QVERIFY(sharedViewChangedSpy.isValid());
    QVERIFY(frameReadySpy.isValid());
    QVERIFY(osdStateChangedSpy.isValid());
    QVERIFY(currentFrameChangedSpy.isValid());
    QVERIFY(currentTimeMsChangedSpy.isValid());
    QVERIFY(metadataInitializedSpy.isValid());
    QVERIFY(componentDisplayModeChangedSpy.isValid());
}

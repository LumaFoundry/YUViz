#include "videoControllerTest.h"
#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <memory>
#include "../../src/controller/compareController.h"
#include "../../src/controller/videoController.h"
#include "../../src/utils/videoFileInfo.h"

void VideoControllerTest::testConstructor() {
    // Create a mock CompareController
    auto compareController = std::make_shared<CompareController>();

    // Test VideoController constructor with empty video files
    VideoController controller(this, compareController);

    // Test default property values
    QCOMPARE(controller.duration(), 0);
    QCOMPARE(controller.totalFrames(), 0);
    QVERIFY(!controller.isPlaying());
    QCOMPARE(controller.currentTimeMs(), 0.0);
    QVERIFY(controller.isForward());
    QVERIFY(!controller.ready());
    QVERIFY(!controller.isSeeking());
    QVERIFY(!controller.isBuffering());
}

void VideoControllerTest::testPlayPause() {
    auto compareController = std::make_shared<CompareController>();
    VideoController controller(this, compareController);

    // Test signal connections for play/pause
    QSignalSpy playTimerSpy(&controller, &VideoController::playTimer);
    QSignalSpy stopTimerSpy(&controller, &VideoController::stopTimer);
    QSignalSpy pauseTimerSpy(&controller, &VideoController::pauseTimer);
    QSignalSpy isPlayingChangedSpy(&controller, &VideoController::isPlayingChanged);

    // Verify signals are connected
    QVERIFY(playTimerSpy.isValid());
    QVERIFY(stopTimerSpy.isValid());
    QVERIFY(pauseTimerSpy.isValid());
    QVERIFY(isPlayingChangedSpy.isValid());

    // Test basic property access (should not crash)
    QVERIFY(!controller.isPlaying());
    QCOMPARE(controller.currentTimeMs(), 0.0);
    QVERIFY(controller.isForward());
}

void VideoControllerTest::testSeek() {
    auto compareController = std::make_shared<CompareController>();
    VideoController controller(this, compareController);

    // Test signal connections for seeking
    QSignalSpy seekTimerSpy(&controller, &VideoController::seekTimer);
    QSignalSpy seekingChangedSpy(&controller, &VideoController::seekingChanged);
    QSignalSpy currentTimeMsChangedSpy(&controller, &VideoController::currentTimeMsChanged);

    // Verify signals are connected
    QVERIFY(seekTimerSpy.isValid());
    QVERIFY(seekingChangedSpy.isValid());
    QVERIFY(currentTimeMsChangedSpy.isValid());

    // Test seek methods (they should not crash)
    controller.seekTo(1000.0); // 1 second
    controller.jumpToFrame(100);
}

void VideoControllerTest::testSpeedControl() {
    auto compareController = std::make_shared<CompareController>();
    VideoController controller(this, compareController);

    // Test signal connections for speed control
    QSignalSpy setSpeedTimerSpy(&controller, &VideoController::setSpeedTimer);

    // Verify signal is connected
    QVERIFY(setSpeedTimerSpy.isValid());

    // Test speed control methods (they should not crash)
    controller.setSpeed(1.0f); // Normal speed
    controller.setSpeed(2.0f); // Double speed
    controller.setSpeed(0.5f); // Half speed
}

void VideoControllerTest::testInvalidOperations() {
    auto compareController = std::make_shared<CompareController>();
    VideoController controller(this, compareController);

    // Test signal connections for invalid operations
    QSignalSpy readyChangedSpy(&controller, &VideoController::readyChanged);
    QSignalSpy isBufferingChangedSpy(&controller, &VideoController::isBufferingChanged);

    // Verify signals are connected
    QVERIFY(readyChangedSpy.isValid());
    QVERIFY(isBufferingChangedSpy.isValid());

    // Test basic property access (should not crash)
    QVERIFY(!controller.ready());
    QVERIFY(!controller.isBuffering());
    QCOMPARE(controller.duration(), 0);
    QCOMPARE(controller.totalFrames(), 0);
}

void VideoControllerTest::testErrorHandling() {
    auto compareController = std::make_shared<CompareController>();
    VideoController controller(this, compareController);

    // Test signal connections for error handling
    QSignalSpy readyChangedSpy(&controller, &VideoController::readyChanged);
    QSignalSpy isBufferingChangedSpy(&controller, &VideoController::isBufferingChanged);
    QSignalSpy directionChangedSpy(&controller, &VideoController::directionChanged);
    QSignalSpy durationChangedSpy(&controller, &VideoController::durationChanged);
    QSignalSpy totalFramesChangedSpy(&controller, &VideoController::totalFramesChanged);

    // Verify signals are connected
    QVERIFY(readyChangedSpy.isValid());
    QVERIFY(isBufferingChangedSpy.isValid());
    QVERIFY(directionChangedSpy.isValid());
    QVERIFY(durationChangedSpy.isValid());
    QVERIFY(totalFramesChangedSpy.isValid());

    // Test basic property access (should not crash)
    QVERIFY(!controller.ready());
    QVERIFY(!controller.isBuffering());
    QCOMPARE(controller.duration(), 0);
    QCOMPARE(controller.totalFrames(), 0);
}

#include <QCoreApplication>
#include <QDebug>
#include <QTest>

// Include all test classes
#include "controller/compareControllerTest.h"
#include "controller/timerTest.h"
#include "controller/videoControllerTest.h"
#include "decoder/videoDecoderTest.h"
#include "rendering/diffRendererTest.h"
#include "rendering/videoRendererTest.h"
#include "ui/videoLoaderTest.h"
#include "ui/videoWindowTest.h"
#include "utils/appConfigTest.h"
#include "utils/errorReporterTest.h"
#include "utils/psnrResultTest.h"
#include "utils/videoFileInfoTest.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // Set up test environment
    qDebug() << "Starting unit tests for Qt6 Video Player";

    // Create test suite
    QTest::qExec(new ErrorReporterTest(), argc, argv);
    QTest::qExec(new AppConfigTest(), argc, argv);
    QTest::qExec(new PSNRResultTest(), argc, argv);
    QTest::qExec(new VideoFileInfoTest(), argc, argv);
    QTest::qExec(new TimerTest(), argc, argv);
    QTest::qExec(new VideoControllerTest(), argc, argv);
    QTest::qExec(new CompareControllerTest(), argc, argv);
    QTest::qExec(new VideoDecoderTest(), argc, argv);
    QTest::qExec(new VideoRendererTest(), argc, argv);
    QTest::qExec(new DiffRendererTest(), argc, argv);
    QTest::qExec(new VideoWindowTest(), argc, argv);
    QTest::qExec(new VideoLoaderTest(), argc, argv);

    return 0;
}

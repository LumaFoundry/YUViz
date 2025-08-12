#include "timerTest.h"
#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

void TimerTest::testConstructor() {
    std::vector<AVRational> timebase = {{1, 25}};
    Timer timer(this, timebase);

    // Test initial status
    QCOMPARE(timer.getStatus(), Paused);
}
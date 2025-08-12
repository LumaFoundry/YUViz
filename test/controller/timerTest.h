#pragma once

#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include "controller/timer.h"

extern "C" {
#include <libavutil/rational.h>
}

class TimerTest : public QObject {
    Q_OBJECT

  private slots:
    // Test constructor and basic properties
    void testConstructor();
};

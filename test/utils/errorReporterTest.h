#pragma once

#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include "utils/errorReporter.h"

class ErrorReporterTest : public QObject {
    Q_OBJECT

  private slots:
    // Test singleton pattern
    void testSingletonInstance();
};

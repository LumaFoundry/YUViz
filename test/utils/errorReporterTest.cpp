#include "errorReporterTest.h"
#include <QDebug>
#include <QSignalSpy>
#include <QTest>

void ErrorReporterTest::testSingletonInstance() {
    // Test that singleton instance is always the same
    ErrorReporter& instance1 = ErrorReporter::instance();
    ErrorReporter& instance2 = ErrorReporter::instance();

    QVERIFY(&instance1 == &instance2);
}
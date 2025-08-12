#include "compareControllerTest.h"
#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <memory>

void CompareControllerTest::testConstructor() {
    // Test that CompareController can be constructed
    CompareController controller(this);
    QVERIFY(&controller != nullptr);
}

#include "psnrResultTest.h"
#include <QDebug>
#include <QTest>
#include "../../src/utils/psnrResult.h"

void PSNRResultTest::testDefaultConstructor() {
    PSNRResult result;

    QCOMPARE(result.average, -1.0);
    QCOMPARE(result.y, -1.0);
    QCOMPARE(result.u, -1.0);
    QCOMPARE(result.v, -1.0);
    QVERIFY(!result.isValid());
}

void PSNRResultTest::testParameterizedConstructor() {
    PSNRResult result(30.5, 32.1, 28.9, 29.2);

    QCOMPARE(result.average, 30.5);
    QCOMPARE(result.y, 32.1);
    QCOMPARE(result.u, 28.9);
    QCOMPARE(result.v, 29.2);
    QVERIFY(result.isValid());
}

void PSNRResultTest::testIsValid() {
    // Test valid PSNR values
    PSNRResult validResult(25.0, 26.0, 24.0, 25.5);
    QVERIFY(validResult.isValid());

    // Test invalid PSNR values (negative)
    PSNRResult invalidResult(-1.0, -1.0, -1.0, -1.0);
    QVERIFY(!invalidResult.isValid());

    // Test partially invalid values
    PSNRResult partialInvalid(25.0, -1.0, 24.0, 25.5);
    QVERIFY(!partialInvalid.isValid());
}

void PSNRResultTest::testToString() {
    PSNRResult result(30.5, 32.1, 28.9, 29.2);
    QString str = result.toString();

    QVERIFY(str.contains("30.5"));
    QVERIFY(str.contains("32.1"));
    QVERIFY(str.contains("28.9"));
    QVERIFY(str.contains("29.2"));
    QVERIFY(str.contains("PSNR"));
}

void PSNRResultTest::testValidPSNRValues() {
    // Test typical PSNR values
    std::vector<double> testValues = {20.0, 25.0, 30.0, 35.0, 40.0};

    for (double value : testValues) {
        PSNRResult result(value, value + 0.5, value - 0.5, value + 0.2);
        QVERIFY(result.isValid());
        QCOMPARE(result.average, value);
    }
}

void PSNRResultTest::testInvalidPSNRValues() {
    // Test negative values
    PSNRResult negativeResult(-5.0, -3.0, -4.0, -2.0);
    QVERIFY(!negativeResult.isValid());

    // Test zero values (should be valid)
    PSNRResult zeroResult(0.0, 0.0, 0.0, 0.0);
    QVERIFY(zeroResult.isValid());
}

void PSNRResultTest::testEdgeCases() {
    // Test very large values
    PSNRResult largeResult(100.0, 101.0, 99.0, 100.5);
    QVERIFY(largeResult.isValid());

    // Test very small positive values
    PSNRResult smallResult(0.001, 0.002, 0.001, 0.001);
    QVERIFY(smallResult.isValid());

    // Test mixed valid/invalid
    PSNRResult mixedResult(25.0, -1.0, -1.0, -1.0);
    QVERIFY(!mixedResult.isValid());
}


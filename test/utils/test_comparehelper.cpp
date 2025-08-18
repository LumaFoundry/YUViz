#include <QtTest>
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "utils/compareHelper.h"
#include "utils/psnrResult.h"

class CompareHelperTest : public QObject {
    Q_OBJECT

  private slots:
    void testIdenticalFrames();
    void testDifferentFrames();
    void testNullPointers();
    void testZeroSizes();
    void testOneNullPlanePointer();
    void testLargeDifference();
    void testDifferentMetadata();
    void testPartialDifference();
    void testLargeBufferCoverage();
};

void CompareHelperTest::testLargeBufferCoverage() {
    auto meta = std::make_shared<FrameMeta>();
    // Y: 40x1 (40), U: 17x1 (17), V: 17x1 (17) => total 74 bytes
    meta->setYWidth(40);
    meta->setYHeight(1);
    meta->setUVWidth(17);
    meta->setUVHeight(1);

    auto buffer1 = std::make_shared<std::vector<uint8_t>>(74, 100);
    auto buffer2 = std::make_shared<std::vector<uint8_t>>(74, 100);

    // Make differences in main loop region (first 32 bytes of Y)
    (*buffer2)[0] = 0;
    (*buffer2)[31] = 0;
    // Make differences in tail loop region (next 8 bytes of Y)
    (*buffer2)[32] = 0;
    (*buffer2)[39] = 0;
    // Make differences in scalar loop region (last byte of U)
    (*buffer2)[56] = 0;

    // Make V the same
    for (int i = 57; i < 74; ++i) {
        (*buffer1)[i] = 100;
        (*buffer2)[i] = 100;
    }

    FrameData frame1(40, 17, buffer1, 0); // ySize=40, uvSize=17
    FrameData frame2(40, 17, buffer2, 0);

    CompareHelper helper;
    PSNRResult result = helper.getPSNR(&frame1, &frame2, meta.get(), meta.get());
    QVERIFY(result.average < 100.0); // Should be finite
    QVERIFY(result.y < 100.0);
    QVERIFY(result.u < 100.0);
    QVERIFY(std::isinf(result.v)); // V plane identical
}

void CompareHelperTest::testIdenticalFrames() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);

    auto buffer = std::make_shared<std::vector<uint8_t>>(8, 100); // All pixels = 100
    FrameData frame1(2, 2, buffer, 0);
    FrameData frame2(2, 2, buffer, 0);

    CompareHelper helper;
    PSNRResult result = helper.getPSNR(&frame1, &frame2, meta.get(), meta.get());
    QVERIFY(std::isinf(result.average));
    QVERIFY(std::isinf(result.y));
    QVERIFY(std::isinf(result.u));
    QVERIFY(std::isinf(result.v));
}

void CompareHelperTest::testDifferentFrames() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);

    auto buffer1 = std::make_shared<std::vector<uint8_t>>(8, 100);
    auto buffer2 = std::make_shared<std::vector<uint8_t>>(8, 110); // All pixels = 110

    FrameData frame1(2, 2, buffer1, 0);
    FrameData frame2(2, 2, buffer2, 0);

    CompareHelper helper;
    PSNRResult result = helper.getPSNR(&frame1, &frame2, meta.get(), meta.get());
    QVERIFY(result.average < 100.0); // Should be finite and not infinity
    QVERIFY(result.y < 100.0);
    QVERIFY(result.u < 100.0);
    QVERIFY(result.v < 100.0);
}

void CompareHelperTest::testNullPointers() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);

    FrameData frame1(2, 2, nullptr, 0); // Null buffer
    FrameData frame2(2, 2, nullptr, 0);

    CompareHelper helper;
    PSNRResult result = helper.getPSNR(&frame1, &frame2, meta.get(), meta.get());
    // All values should be zero (default constructed PSNRResult)
    QVERIFY(result.average == -1.0);
    QVERIFY(result.y == -1.0);
    QVERIFY(result.u == -1.0);
    QVERIFY(result.v == -1.0);
}

void CompareHelperTest::testZeroSizes() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(0);
    meta->setYHeight(0);
    meta->setUVWidth(0);
    meta->setUVHeight(0);

    auto buffer = std::make_shared<std::vector<uint8_t>>(0);
    FrameData frame1(0, 0, buffer, 0);
    FrameData frame2(0, 0, buffer, 0);

    CompareHelper helper;
    PSNRResult result = helper.getPSNR(&frame1, &frame2, meta.get(), meta.get());
    QVERIFY(result.average == -1.0);
    QVERIFY(result.y == -1.0);
    QVERIFY(result.u == -1.0);
    QVERIFY(result.v == -1.0);
}

void CompareHelperTest::testOneNullPlanePointer() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);

    auto buffer = std::make_shared<std::vector<uint8_t>>(8, 100);
    FrameData frame1(2, 2, buffer, 0);
    FrameData frame2(2, 2, nullptr, 0); // Null buffer for frame2

    CompareHelper helper;
    PSNRResult result = helper.getPSNR(&frame1, &frame2, meta.get(), meta.get());
    QVERIFY(result.average == -1.0);
    QVERIFY(result.y == -1.0);
    QVERIFY(result.u == -1.0);
    QVERIFY(result.v == -1.0);
}

void CompareHelperTest::testLargeDifference() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);

    auto buffer1 = std::make_shared<std::vector<uint8_t>>(8, 0);
    auto buffer2 = std::make_shared<std::vector<uint8_t>>(8, 255);

    FrameData frame1(2, 2, buffer1, 0);
    FrameData frame2(2, 2, buffer2, 0);

    CompareHelper helper;
    PSNRResult result = helper.getPSNR(&frame1, &frame2, meta.get(), meta.get());
    QVERIFY(result.average < 10.0); // Should be very low PSNR
    QVERIFY(result.y < 10.0);
    QVERIFY(result.u < 10.0);
    QVERIFY(result.v < 10.0);
}

void CompareHelperTest::testDifferentMetadata() {
    auto meta1 = std::make_shared<FrameMeta>();
    meta1->setYWidth(2);
    meta1->setYHeight(2);
    meta1->setUVWidth(1);
    meta1->setUVHeight(1);

    auto meta2 = std::make_shared<FrameMeta>();
    meta2->setYWidth(2);
    meta2->setYHeight(2);
    meta2->setUVWidth(2); // Different UV size
    meta2->setUVHeight(1);

    auto buffer1 = std::make_shared<std::vector<uint8_t>>(8, 100);
    auto buffer2 = std::make_shared<std::vector<uint8_t>>(8, 100);

    FrameData frame1(2, 2, buffer1, 0);
    FrameData frame2(2, 2, buffer2, 0);

    CompareHelper helper;
    PSNRResult result = helper.getPSNR(&frame1, &frame2, meta1.get(), meta2.get());
    QVERIFY(std::isinf(result.average));
    QVERIFY(std::isinf(result.y));
    QVERIFY(std::isinf(result.u));
    QVERIFY(std::isinf(result.v));
}

void CompareHelperTest::testPartialDifference() {
    auto meta = std::make_shared<FrameMeta>();
    meta->setYWidth(2);
    meta->setYHeight(2);
    meta->setUVWidth(1);
    meta->setUVHeight(1);

    // Y plane identical, U plane different, V plane identical
    auto buffer1 = std::make_shared<std::vector<uint8_t>>(8, 100);
    auto buffer2 = std::make_shared<std::vector<uint8_t>>(8, 100);

    // Y plane: indices 0-3, U plane: index 4, V plane: index 5 (for 2x2 Y, 1x1 U, 1x1 V)
    // Set U plane different
    (*buffer1)[4] = 100;
    (*buffer2)[4] = 0; // U plane different
    // V plane identical
    (*buffer1)[5] = 100;
    (*buffer2)[5] = 100;

    FrameData frame1(4, 2, buffer1, 0); // ySize=4, uvSize=2
    FrameData frame2(4, 2, buffer2, 0);

    CompareHelper helper;
    PSNRResult result = helper.getPSNR(&frame1, &frame2, meta.get(), meta.get());
    QVERIFY(std::isinf(result.y));        // Y plane identical, so PSNR should be infinity or very high
    QVERIFY(!std::isinf(result.u));       // U plane different, so PSNR should be finite
    QVERIFY(std::isinf(result.v));        // V plane identical, so PSNR should be infinity or very high
    QVERIFY(!std::isinf(result.average)); // Average should be finite
}

QTEST_MAIN(CompareHelperTest)
#include "test_comparehelper.moc"
#include "appConfigTest.h"
#include <QDebug>
#include <QFuture>
#include <QTest>
#include <QThread>
#include <QtConcurrent>
#include "../../src/utils/appConfig.h"

void AppConfigTest::testSingletonInstance() {
    // Test that instance() returns the same object
    AppConfig& config1 = AppConfig::instance();
    AppConfig& config2 = AppConfig::instance();

    QVERIFY(&config1 == &config2);
}

void AppConfigTest::testDefaultQueueSize() {
    AppConfig& config = AppConfig::instance();

    // Test default queue size
    QCOMPARE(config.getQueueSize(), 50);
}

void AppConfigTest::testSetQueueSize() {
    AppConfig& config = AppConfig::instance();

    // Test setting queue size
    config.setQueueSize(100);
    QCOMPARE(config.getQueueSize(), 100);

    // Reset to default
    config.setQueueSize(50);
}

void AppConfigTest::testGetQueueSize() {
    AppConfig& config = AppConfig::instance();

    // Test getting queue size
    int size = config.getQueueSize();
    QVERIFY(size > 0);
    QVERIFY(size <= 1000); // Reasonable upper limit
}

void AppConfigTest::testQueueSizeModification() {
    AppConfig& config = AppConfig::instance();

    // Test various queue sizes
    std::vector<int> testSizes = {10, 25, 50, 100, 200};

    for (int size : testSizes) {
        config.setQueueSize(size);
        QCOMPARE(config.getQueueSize(), size);
    }

    // Reset to default
    config.setQueueSize(50);
}

void AppConfigTest::testConcurrentAccess() {
    AppConfig& config = AppConfig::instance();

    // Test concurrent access to the singleton
    QFuture<void> future1 = QtConcurrent::run([&config]() {
        for (int i = 0; i < 10; ++i) {
            config.setQueueSize(i * 10);
            QThread::msleep(1);
        }
    });

    QFuture<void> future2 = QtConcurrent::run([&config]() {
        for (int i = 0; i < 10; ++i) {
            int size = config.getQueueSize();
            QVERIFY(size >= 0);
            QThread::msleep(1);
        }
    });

    future1.waitForFinished();
    future2.waitForFinished();

    // Reset to default
    config.setQueueSize(50);
}


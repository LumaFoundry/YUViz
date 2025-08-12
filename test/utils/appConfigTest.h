#pragma once

#include <QObject>
#include <QTest>

class AppConfigTest : public QObject {
    Q_OBJECT

  private slots:
    void testSingletonInstance();
    void testDefaultQueueSize();
    void testSetQueueSize();
    void testGetQueueSize();
    void testQueueSizeModification();
    void testConcurrentAccess();
};

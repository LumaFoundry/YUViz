#pragma once

#include <QObject>
#include <QTest>
#include "controller/videoController.h"

class VideoControllerTest : public QObject {
    Q_OBJECT

  private slots:
    // Test basic functionality
    void testConstructor();
    void testPlayPause();
    void testSeek();
    void testSpeedControl();

    // Test edge cases
    void testInvalidOperations();
    void testErrorHandling();
};


#pragma once

#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include "controller/compareController.h"
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "frames/frameQueue.h"

class CompareControllerTest : public QObject {
    Q_OBJECT

  private slots:
    // Basic functionality tests
    void testConstructor();
};

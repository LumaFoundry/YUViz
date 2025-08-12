#pragma once

#include <QObject>
#include <QTest>
#include "rendering/videoRenderer.h"

class VideoRendererTest : public QObject {
    Q_OBJECT

  private slots:
    void testConstructor();
    void testRendering();
    void testErrorHandling();
};


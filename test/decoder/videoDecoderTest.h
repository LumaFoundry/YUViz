#pragma once

#include <QObject>
#include <QTest>
#include "decoder/videoDecoder.h"

class VideoDecoderTest : public QObject {
    Q_OBJECT

  private slots:
    void testConstructor();
    void testDecoding();
    void testErrorHandling();
};


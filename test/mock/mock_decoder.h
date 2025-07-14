#pragma once

#include <gmock/gmock.h>
#include "decoder/videoDecoder.h"

class MockDecoder : public VideoDecoder {
    Q_OBJECT
  public:
    MockDecoder(QObject* parent = nullptr) :
        VideoDecoder(parent) {}

    MOCK_METHOD(FrameMeta, getMetaData, (), (override));

    MOCK_METHOD(void, loadFrame, (FrameData*), (override));
};

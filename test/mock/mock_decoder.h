#pragma once

#include "decoder/videoDecoder.h"
#include <gmock/gmock.h>

class MockDecoder : public VideoDecoder {
    Q_OBJECT
public:
    MockDecoder(QObject* parent = nullptr) : VideoDecoder(parent) {}

    MOCK_METHOD(FrameMeta, getMetaData, (), (override));

    MOCK_METHOD(void, loadFrame, (FrameData*), (override));

};

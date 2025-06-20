#pragma once

#include "rendering/videoRenderer.h"
#include <gmock/gmock.h>

class MockRenderer : public VideoRenderer {
    Q_OBJECT
public:
    MockRenderer(QObject* parent = nullptr) : VideoRenderer(parent) {}

    MOCK_METHOD(void, uploadFrame, (FrameData*), (override));

    MOCK_METHOD(void, renderFrame, (), (override));
};

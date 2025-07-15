#pragma once

#include <gmock/gmock.h>
#include "rendering/videoRenderer.h"

class MockRenderer : public VideoRenderer {
    Q_OBJECT
  public:
    MockRenderer(QObject* parent = nullptr,
                 std::shared_ptr<VideoWindow> window = nullptr,
                 std::shared_ptr<FrameMeta> metaPtr = nullptr) :
        VideoRenderer(parent, window, metaPtr) {}

    MOCK_METHOD(void, uploadFrame, (FrameData*), (override));

    MOCK_METHOD(void, renderFrame, (), (override));
};

#pragma once

#include <QSGRenderNode>
#include <QQuickWindow>
#include <QRect>
#include "rhi/qrhi.h"
#include "videoRenderer.h"

class VideoRenderNode : public QSGRenderNode {
public:
    explicit VideoRenderNode(QQuickWindow *window, VideoRenderer *renderer);
    ~VideoRenderNode() override = default;

    void prepare() override;
    void render(const RenderState *state) override;
    RenderingFlags flags() const override {
        return RenderingFlag::BoundedRectRendering;
    }

private:
    QQuickWindow *m_window;
    VideoRenderer *m_renderer;
    bool m_initialized = false;
};
#pragma once

#include <QQuickWindow>
#include <QRect>
#include <QSGRenderNode>
#include "rhi/qrhi.h"
#include "videoRenderer.h"

class VideoRenderNode : public QSGRenderNode {
  public:
    explicit VideoRenderNode(QQuickItem* item, VideoRenderer* renderer);
    ~VideoRenderNode() override = default;

    void prepare() override;
    void render(const RenderState* state) override;
    RenderingFlags flags() const override { return RenderingFlag::BoundedRectRendering; }
    QRectF rect() const override;

  private:
    QQuickItem* m_item;
    VideoRenderer* m_renderer;
    bool m_initialized = false;
};
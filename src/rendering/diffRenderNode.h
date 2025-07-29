#pragma once

#include <QQuickWindow>
#include <QRect>
#include <QSGRenderNode>
#include "diffRenderer.h"
#include "rhi/qrhi.h"

class DiffRenderNode : public QSGRenderNode {
  public:
    explicit DiffRenderNode(QQuickItem* item, DiffRenderer* renderer);
    ~DiffRenderNode() override = default;

    void prepare() override;
    void render(const RenderState* state) override;
    RenderingFlags flags() const override { return RenderingFlag::BoundedRectRendering; }
    QRectF rect() const override;

  private:
    QQuickItem* m_item;
    DiffRenderer* m_renderer;
    bool m_initialized = false;
};
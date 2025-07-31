#include "diffRenderNode.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QRect>
#include <QRectF>

DiffRenderNode::DiffRenderNode(QQuickItem* item, DiffRenderer* renderer) :
    m_item(item),
    m_renderer(renderer) {
}

QRectF DiffRenderNode::rect() const {
    if (!m_item)
        return QRectF();
    return QRectF(0, 0, m_item->width(), m_item->height());
}

void DiffRenderNode::prepare() {
    if (m_initialized) {
        return;
    }
    QRhi* rhi = m_item->window()->rhi();
    if (!rhi) {
        return;
    }
    QRhiRenderPassDescriptor* rp = renderTarget()->renderPassDescriptor();
    m_renderer->initialize(rhi, rp);
    m_initialized = true;
}

void DiffRenderNode::render(const RenderState* state) {
    QRhiCommandBuffer* cb = commandBuffer();
    QRectF boundsF = rect();
    QPointF topLeft = m_item->mapToScene(QPointF(0, 0));
    qreal dpr = m_item->window()->devicePixelRatio();
    int windowHeightPx = m_item->window()->height() * dpr;
    int x = topLeft.x() * dpr;
    int y = windowHeightPx - (topLeft.y() + m_item->height()) * dpr;
    int w = m_item->width() * dpr;
    int h = m_item->height() * dpr;
    QRect viewportRect(x, y, w, h);
    cb->setViewport(QRhiViewport(viewportRect.x(), viewportRect.y(), viewportRect.width(), viewportRect.height()));
    m_renderer->renderFrame(cb, viewportRect, renderTarget());
}
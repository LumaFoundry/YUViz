#include "videoRenderNode.h"
#include <QQuickWindow>
#include <QRect>

VideoRenderNode::VideoRenderNode(QQuickWindow *window, VideoRenderer *renderer)
    : m_window(window)
    , m_renderer(renderer) {}

void VideoRenderNode::prepare() {
    if (m_initialized) {
        return;
    }
    QRhi *rhi = m_window->rhi();
    QRhiRenderPassDescriptor *rp = renderTarget()->renderPassDescriptor();
    m_renderer->initialize(rhi, rp);
    m_initialized = true;
}

void VideoRenderNode::render(const RenderState *state) {
    qDebug() << "VideoRenderNode::render called";
    QRhiCommandBuffer *cb = commandBuffer();
    QRect viewport = state->scissorRect();
    viewport.setY(m_window->height() - viewport.y() - viewport.height());
    cb->setViewport(QRhiViewport(
        viewport.x(),
        viewport.y(),
        viewport.width(),
        viewport.height()
    ));
    cb->setScissor({ viewport.x(), viewport.y(), viewport.width(), viewport.height() });
    m_renderer->renderFrame(cb, viewport, renderTarget());
}
#include "videoRenderNode.h"
#include <QQuickWindow>
#include <QRect>
#include <QQuickItem>

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
    // qDebug() << "VideoRenderNode::render called";
    QRhiCommandBuffer *cb = commandBuffer();

    QSizeF logicalSize = m_window->contentItem()->size();
    qreal dpr = m_window->devicePixelRatio();

    // Calculate the physical size in pixels by scaling with the DPR.
    int physicalWidth = static_cast<int>(logicalSize.width() * dpr);
    int physicalHeight = static_cast<int>(logicalSize.height() * dpr);

    QRect physicalViewport(0, 0, physicalWidth, physicalHeight);
    cb->setViewport(QRhiViewport(
        physicalViewport.x(),
        physicalViewport.y(),
        physicalViewport.width(),
        physicalViewport.height()
    ));
    cb->setScissor(QRhiScissor(
        physicalViewport.x(),
        physicalViewport.y(),
        physicalViewport.width(),
        physicalViewport.height()
    ));

    m_renderer->renderFrame(cb, physicalViewport, renderTarget());
}
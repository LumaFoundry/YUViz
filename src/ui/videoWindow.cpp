#include "videoWindow.h"
#include "frames/frameData.h"
#include "rendering/videoRenderer.h"
#include "rendering/videoRenderNode.h"
#include "frames/frameMeta.h"

VideoWindow::VideoWindow(QQuickItem *parent):
    QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void VideoWindow::initialize(std::shared_ptr<FrameMeta> metaPtr) {
    m_renderer = new VideoRenderer(this, metaPtr);
    // connect(m_renderer, &VideoRenderer::batchIsFull, this, &VideoWindow::batchIsFull);
    // connect(m_renderer, &VideoRenderer::batchIsEmpty, this, &VideoWindow::batchIsEmpty);
    // connect(m_renderer, &VideoRenderer::rendererError, this, &VideoWindow::rendererError);
    if (window()) {
        update();
    } else {
        connect(this, &QQuickItem::windowChanged, this, [=](QQuickWindow *win){
            qDebug() << "[VideoWindow] window became available, calling update()";
            update();
        });
    }
}

void VideoWindow::uploadFrame(FrameData* frame) {
    // qDebug() << "VideoWindow::uploadFrame called in thread";
    m_renderer->uploadFrame(frame);
}

void VideoWindow::renderFrame() {
    // qDebug() << "VideoWindow::renderFrame called in thread" << QThread::currentThread();
    update();
}

void VideoWindow::setColorParams(AVColorSpace space, AVColorRange range) {
    m_renderer->setColorParams(space, range);
}

void VideoWindow::releaseBatch() {
    m_renderer->releaseBatch();
}

void VideoWindow::batchIsFull() {
    emit batchUploaded(true);
}

void VideoWindow::batchIsEmpty() {
    emit gpuUploaded(true);
}

void VideoWindow::rendererError() {
    emit errorOccurred();
}

QSGNode *VideoWindow::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    // qDebug() << "VideoWindow::updatePaintNode called in thread" << QThread::currentThread();
    
    if (!m_renderer) {
        return nullptr;
    }
    VideoRenderNode *node = static_cast<VideoRenderNode *>(oldNode);
    if (!node) {
        node = new VideoRenderNode(window(), m_renderer);
    }
    return node;
}

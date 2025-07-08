#include "videoWindow.h"
#include "frames/frameData.h"
#include "rendering/videoRenderer.h"
#include "rendering/videoRenderNode.h"
#include "frames/frameMeta.h"
#include <QMouseEvent>

VideoWindow::VideoWindow(QQuickItem *parent):
    QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
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
    m_renderer->uploadFrame(frame);
}

void VideoWindow::renderFrame() {
    qDebug() << "VideoWindow::renderFrame called in thread" << QThread::currentThread();
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
    qDebug() << "VideoWindow::updatePaintNode called in thread" << QThread::currentThread();
    
    if (!m_renderer) {
        return nullptr;
    }
    VideoRenderNode *node = static_cast<VideoRenderNode *>(oldNode);
    if (!node) {
        node = new VideoRenderNode(window(), m_renderer);
    }
    return node;
}

void VideoWindow::setZoom(qreal zoom) {
    if (m_renderer) {
        m_renderer->setZoomFactor(static_cast<float>(zoom));
        update();
    }
}

void VideoWindow::setSelectionRect(const QRectF &rect) {
    m_selectionRect = rect;
    m_hasSelection = !rect.isNull();
    emit selectionChanged(rect);
    update();
}

void VideoWindow::clearSelection() {
    m_selectionRect = QRectF();
    m_hasSelection = false;
    m_isSelecting = false;
    m_isZoomed = false;
    m_currentZoomRect = QRectF();
    emit selectionChanged(QRectF());
    emit zoomChanged();
    update();
}

void VideoWindow::resetZoom() {
    // Reset zoom state, restore to full screen display
    m_isZoomed = false;
    m_currentZoomRect = QRectF();

    // Clear selection state
    m_selectionRect = QRectF();
    m_hasSelection = false;
    m_isSelecting = false;

    // Reset renderer zoom parameters
    if (m_renderer) {
        m_renderer->setZoomAndOffset(QRectF(0, 0, 1, 1));
    }

    emit selectionChanged(QRectF());
    emit zoomChanged();
    update();

    qDebug() << "Reset zoom - restore to full screen display";
}

void VideoWindow::zoomToSelection() {
    if (!m_renderer) return;

    QRectF itemRect = boundingRect();
    if (itemRect.isEmpty()) return;

    // Strict validation of input rectangle
    if (m_selectionRect.width() <= 0 || m_selectionRect.height() <= 0 ||
        std::isnan(m_selectionRect.x()) || std::isnan(m_selectionRect.y()) ||
        std::isnan(m_selectionRect.width()) || std::isnan(m_selectionRect.height())) {
        return;
    }

    // Simplified approach: always convert window coordinates to normalized coordinates
    // For continuous zoom, user-drawn rectangles are limited to current display area
    QRectF clampedRect = m_selectionRect.intersected(itemRect);
    if (clampedRect.width() <= 1e-6 || clampedRect.height() <= 1e-6) return;

    QRectF normalizedSelection(
        clampedRect.x() / itemRect.width(),
        clampedRect.y() / itemRect.height(),
        clampedRect.width() / itemRect.width(),
        clampedRect.height() / itemRect.height()
    );

    // Strict validity check
    if (normalizedSelection.width() <= 1e-6 || normalizedSelection.height() <= 1e-6 ||
        normalizedSelection.x() < 0 || normalizedSelection.y() < 0 ||
        normalizedSelection.x() + normalizedSelection.width() > 1.0 ||
        normalizedSelection.y() + normalizedSelection.height() > 1.0 ||
        std::isnan(normalizedSelection.x()) || std::isnan(normalizedSelection.y()) ||
        std::isnan(normalizedSelection.width()) || std::isnan(normalizedSelection.height())) {
        return;
    }

    // Prevent excessive zoom
    if (normalizedSelection.width() < 0.01 || normalizedSelection.height() < 0.01) {
        return;
    }

    // If currently zoomed, need to map selection area to relative coordinates within current zoom area
    if (m_isZoomed && !m_currentZoomRect.isNull()) {
        // Map selection area to relative coordinates within current zoom area
        QRectF relativeSelection(
            (normalizedSelection.x() - m_currentZoomRect.x()) / m_currentZoomRect.width(),
            (normalizedSelection.y() - m_currentZoomRect.y()) / m_currentZoomRect.height(),
            normalizedSelection.width() / m_currentZoomRect.width(),
            normalizedSelection.height() / m_currentZoomRect.height()
        );

        // Limit to [0,1] range
        relativeSelection = QRectF(
            qMax(0.0, qMin(1.0, relativeSelection.x())),
            qMax(0.0, qMin(1.0, relativeSelection.y())),
            qMax(0.01, qMin(1.0, relativeSelection.width())),
            qMax(0.01, qMin(1.0, relativeSelection.height()))
        );

        // Map relative coordinates back to global coordinates
        normalizedSelection = QRectF(
            m_currentZoomRect.x() + relativeSelection.x() * m_currentZoomRect.width(),
            m_currentZoomRect.y() + relativeSelection.y() * m_currentZoomRect.height(),
            relativeSelection.width() * m_currentZoomRect.width(),
            relativeSelection.height() * m_currentZoomRect.height()
        );
    }

    m_currentZoomRect = normalizedSelection;
    m_isZoomed = true;
    m_hasSelection = true;  // Set selection state
    m_renderer->setZoomAndOffset(normalizedSelection);
    emit zoomChanged();
    update();

    qDebug() << "Zoom parameters - selection area:" << normalizedSelection;
}

void VideoWindow::mousePressEvent(QMouseEvent *event) {
    // Mouse event handling is done by MouseArea in QML
    QQuickItem::mousePressEvent(event);
}

void VideoWindow::mouseMoveEvent(QMouseEvent *event) {
    // Mouse event handling is done by MouseArea in QML
    QQuickItem::mouseMoveEvent(event);
}

void VideoWindow::mouseReleaseEvent(QMouseEvent *event) {
    // Mouse event handling is done by MouseArea in QML
    QQuickItem::mouseReleaseEvent(event);
}

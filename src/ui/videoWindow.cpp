#include "videoWindow.h"
#include "frames/frameData.h"
#include "rendering/videoRenderer.h"
#include "rendering/videoRenderNode.h"
#include "frames/frameMeta.h"
#include <QMouseEvent>
#include <QtMath>

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

void VideoWindow::setAspectRatio(int width, int height)
{
    if (height > 0) {
        m_videoAspectRatio = static_cast<qreal>(width) / height;
    }
}

qreal VideoWindow::getAspectRatio() const
{
    return m_videoAspectRatio;
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

void VideoWindow::zoomAt(qreal factor, const QPointF &centerPoint) {
    if (!m_renderer) return;

    const QRectF itemRect = boundingRect();
    if (itemRect.isEmpty()) return;

    const QRectF currentRect = (m_isZoomed && !m_currentZoomRect.isNull()) ? m_currentZoomRect : QRectF(0, 0, 1, 1);

    // Calculate the new view dimensions
    qreal newWidth = currentRect.width() / factor;
    qreal newHeight = currentRect.height() / factor;

    // If zooming out past the original view, just reset.
    if (factor < 1.0 && (newWidth > 1.0 || newHeight > 1.0)) {
        resetZoom();
        return;
    }

    // Limit zoom-in
    constexpr qreal maxZoomFactor = 1000.0;
    if (factor > 1.0 && (newWidth < 1.0 / maxZoomFactor || newHeight < 1.0 / maxZoomFactor)) {
        return;
    }

    // Normalize the cursor position to [0, 1] relative to the window
    const QPointF normCenter(
        qBound(0.0, centerPoint.x() / itemRect.width(), 1.0),
        qBound(0.0, centerPoint.y() / itemRect.height(), 1.0)
    );

    // Calculate the new top-left corner to keep the point under the cursor stationary
    const qreal newX = currentRect.x() + normCenter.x() * currentRect.width() * (1.0 - 1.0 / factor);
    const qreal newY = currentRect.y() + normCenter.y() * currentRect.height() * (1.0 - 1.0 / factor);

    QRectF newZoomRect(newX, newY, newWidth, newHeight);

    // Clamp the new rectangle to the bounds of the video
    if (newZoomRect.x() < 0.0) newZoomRect.setX(0.0);
    if (newZoomRect.y() < 0.0) newZoomRect.setY(0.0);
    if (newZoomRect.right() > 1.0) newZoomRect.setX(1.0 - newZoomRect.width());
    if (newZoomRect.bottom() > 1.0) newZoomRect.setY(1.0 - newZoomRect.height());

    m_currentZoomRect = newZoomRect;

    if (qFuzzyCompare(m_currentZoomRect.width(), 1.0) && qFuzzyCompare(m_currentZoomRect.height(), 1.0)) {
        m_currentZoomRect.setRect(0,0,1,1);
        m_isZoomed = false;
    } else {
        m_isZoomed = true;
    }
    
    m_renderer->setZoomAndOffset(m_currentZoomRect);
    emit zoomChanged();
    update();
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
    m_currentZoomRect = QRectF(0,0,1,1);

    // Clear selection state
    m_selectionRect = QRectF();
    m_hasSelection = false;
    m_isSelecting = false;

    // Reset renderer zoom parameters
    if (m_renderer) {
        m_renderer->setZoomAndOffset(m_currentZoomRect);
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

void VideoWindow::pan(const QPointF &delta) {
    if (!m_isZoomed || !m_renderer) return;

    const QRectF itemRect = boundingRect();
    if (itemRect.isEmpty()) return;

    // Pixel delta to a normalized delta, scaled by the current zoom level.
    qreal dx = (-delta.x() / itemRect.width()) * m_currentZoomRect.width();
    qreal dy = (-delta.y() / itemRect.height()) * m_currentZoomRect.height();

    // Calculate the maximum distance we can pan in any direction before hitting an edge.
    const qreal dx_min = -m_currentZoomRect.x();
    const qreal dx_max = 1.0 - m_currentZoomRect.right();
    const qreal dy_min = -m_currentZoomRect.y();
    const qreal dy_max = 1.0 - m_currentZoomRect.bottom();

    // Clamp the delta to the allowed range.
    dx = qBound(dx_min, dx, dx_max);
    dy = qBound(dy_min, dy, dy_max);
    
    // Only update if there's any movement left after clamping.
    if (qFuzzyCompare(dx, 0) && qFuzzyCompare(dy, 0)) {
        return;
    }

    m_currentZoomRect.translate(dx, dy);    
    m_renderer->setZoomAndOffset(m_currentZoomRect);
    
    emit zoomChanged();
    update();
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

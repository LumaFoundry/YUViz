#include "videoWindow.h"
#include <QMouseEvent>
#include <QtMath>
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "rendering/videoRenderNode.h"
#include "rendering/videoRenderer.h"

VideoWindow::VideoWindow(QQuickItem* parent) :
    QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
}

void VideoWindow::initialize(std::shared_ptr<FrameMeta> metaPtr) {
    m_renderer = new VideoRenderer(this, metaPtr);
    if (window()) {
        update();
    } else {
        connect(this, &QQuickItem::windowChanged, this, [=](QQuickWindow* win) {
            qDebug() << "[VideoWindow] window became available, calling update()";
            update();
        });
    }
}

void VideoWindow::setAspectRatio(int width, int height) {
    if (height > 0) {
        m_videoAspectRatio = static_cast<qreal>(width) / height;
    }
}

qreal VideoWindow::getAspectRatio() const {
    return m_videoAspectRatio;
}

void VideoWindow::uploadFrame(FrameData* frame) {
    // qDebug() << "VideoWindow::uploadFrame called in thread";
    m_renderer->releaseBatch();
    m_renderer->uploadFrame(frame);
}

void VideoWindow::renderFrame() {
    // qDebug() << "VideoWindow::renderFrame called in thread" << QThread::currentThread();
    update();
}

void VideoWindow::setColorParams(AVColorSpace space, AVColorRange range) {
    m_renderer->setColorParams(space, range);
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

QSGNode* VideoWindow::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    // qDebug() << "VideoWindow::updatePaintNode called in thread" << QThread::currentThread();

    if (!m_renderer) {
        return nullptr;
    }
    VideoRenderNode* node = static_cast<VideoRenderNode*>(oldNode);
    if (!node) {
        node = new VideoRenderNode(window(), m_renderer);
    }
    return node;
}

QPointF VideoWindow::convertToVideoCoordinates(const QPointF& point) const {
    QRectF itemRect = QRectF(0, 0, window()->width(), window()->height());
    QRectF videoRect = itemRect;
    qreal windowAspect = itemRect.width() / itemRect.height();
    qreal videoAspect = getAspectRatio();
    if (windowAspect > videoAspect) {
        qreal newWidth = videoAspect * itemRect.height();
        videoRect.setX((itemRect.width() - newWidth) / 2.0);
        videoRect.setWidth(newWidth);
    } else {
        qreal newHeight = itemRect.width() / videoAspect;
        videoRect.setY((itemRect.height() - newHeight) / 2.0);
        videoRect.setHeight(newHeight);
    }

    qreal widthMargin = (itemRect.width() - videoRect.width()) / 2.0;
    qreal heightMargin = (itemRect.height() - videoRect.height()) / 2.0;

    return QPointF(qBound(0.0, (point.x() - widthMargin) / videoRect.width(), 1.0),
                   qBound(0.0, (point.y() - heightMargin) / videoRect.height(), 1.0));
}

qreal VideoWindow::maxZoom() const {
    return m_maxZoom;
}

void VideoWindow::setMaxZoom(qreal zoom) {
    if (qFuzzyCompare(m_maxZoom, zoom) || zoom <= 0)
        return;
    m_maxZoom = zoom;
    emit maxZoomChanged();
}

void VideoWindow::zoomAt(qreal factor, const QPointF& centerPoint) {
    if (!m_renderer)
        return;

    const QRectF itemRect = boundingRect();
    if (itemRect.isEmpty())
        return;

    qreal newZoom = qBound(1.0f, m_zoom * factor, m_maxZoom);
    qreal shiftCoef = 1.0f / m_zoom - 1.0f / newZoom;

    // Normalize the cursor position to [0, 1] relative to the window
    const QPointF normCenter = convertToVideoCoordinates(centerPoint);
    m_centerX = m_centerX + (normCenter.x() - 0.5) * shiftCoef;
    m_centerY = m_centerY + (normCenter.y() - 0.5) * shiftCoef;

    m_zoom = newZoom;
    m_isZoomed = (m_zoom > 1.0f);
    if (!m_isZoomed) {
        resetZoom();
    }

    m_renderer->setZoomAndOffset(m_zoom, m_centerX, m_centerY);
    emit zoomChanged();
    update();
}

void VideoWindow::setSelectionRect(const QRectF& rect) {
    m_selectionRect = rect;
    m_hasSelection = !rect.isNull();
    update();
}

void VideoWindow::clearSelection() {
    m_selectionRect = QRectF();
    m_hasSelection = false;
    m_isSelecting = false;
    m_isZoomed = false;
    emit zoomChanged();
    update();
}

void VideoWindow::resetZoom() {
    m_isZoomed = false;
    m_zoom = 1.0f;
    m_centerX = 0.5f;
    m_centerY = 0.5f;
    m_selectionRect = QRectF();
    m_hasSelection = false;
    m_isSelecting = false;
    if (m_renderer) {
        m_renderer->setZoomAndOffset(m_zoom, m_centerX, m_centerY);
    }
    emit zoomChanged();
    update();
    qDebug() << "Reset zoom - restore to full screen display";
}

void VideoWindow::zoomToSelection() {
    if (!m_renderer)
        return;
    QRectF itemRect = QRectF(0, 0, window()->width(), window()->height());
    if (itemRect.isEmpty())
        return;
    if (m_selectionRect.width() <= 0 || m_selectionRect.height() <= 0)
        return;

    // Calculate current viewport rectangle (based on current zoom and center)
    QRectF currentViewRect;
    if (m_isZoomed) {
        float halfView = 0.5f / m_zoom;
        currentViewRect = QRectF(m_centerX - halfView, m_centerY - halfView, 1.0f / m_zoom, 1.0f / m_zoom);
    } else {
        currentViewRect = QRectF(0, 0, 1, 1);
    }

    // Calculate video display area in window (considering letterboxing)
    QRectF videoRect = itemRect;
    qreal windowAspect = itemRect.width() / itemRect.height();
    qreal videoAspect = getAspectRatio();
    if (windowAspect > videoAspect) {
        qreal newWidth = videoAspect * itemRect.height();
        videoRect.setX((itemRect.width() - newWidth) / 2.0);
        videoRect.setWidth(newWidth);
    } else {
        qreal newHeight = itemRect.width() / videoAspect;
        videoRect.setY((itemRect.height() - newHeight) / 2.0);
        videoRect.setHeight(newHeight);
    }

    qreal widthFactor = m_selectionRect.width() / videoRect.width();
    qreal heightFactor = m_selectionRect.height() / videoRect.height();
    if (widthFactor > heightFactor) {
        m_selectionRect.setY(m_selectionRect.y() - (widthFactor * videoRect.height() - m_selectionRect.height()) / 2.0);
        m_selectionRect.setHeight(widthFactor * videoRect.height());
    } else {
        m_selectionRect.setX(m_selectionRect.x() - (heightFactor * videoRect.width() - m_selectionRect.width()) / 2.0);
        m_selectionRect.setWidth(heightFactor * videoRect.width());
    }

    // Convert selection rectangle from window coordinates to current viewport coordinates
    QRectF selectionInView;
    selectionInView.setX((m_selectionRect.x() - videoRect.x()) / videoRect.width());
    selectionInView.setY((m_selectionRect.y() - videoRect.y()) / videoRect.height());
    selectionInView.setWidth(m_selectionRect.width() / videoRect.width());
    selectionInView.setHeight(m_selectionRect.height() / videoRect.height());

    // Map selection rectangle to current viewport coordinate system
    QRectF mappedSelection;
    mappedSelection.setX(currentViewRect.x() + selectionInView.x() * currentViewRect.width());
    mappedSelection.setY(currentViewRect.y() + selectionInView.y() * currentViewRect.height());
    mappedSelection.setWidth(selectionInView.width() * currentViewRect.width());
    mappedSelection.setHeight(selectionInView.height() * currentViewRect.height());

    // Limit selection rectangle within current viewport bounds
    QRectF clampedRect = mappedSelection.intersected(currentViewRect);
    if (clampedRect.width() <= 1e-6 || clampedRect.height() <= 1e-6)
        return;

    // Calculate new zoom and center point
    qreal newZoom = m_zoom * (currentViewRect.width() / clampedRect.width());
    newZoom = qBound(1.0f, newZoom, m_maxZoom);

    m_centerX = clampedRect.x() + clampedRect.width() * 0.5f;
    m_centerY = clampedRect.y() + clampedRect.height() * 0.5f;
    m_zoom = newZoom;

    m_isZoomed = (m_zoom > 1.0f);
    m_renderer->setZoomAndOffset(m_zoom, m_centerX, m_centerY);
    emit zoomChanged();
    update();
}

void VideoWindow::pan(const QPointF& delta) {
    if (!m_isZoomed || !m_renderer)
        return;

    const QRectF itemRect = boundingRect();
    if (itemRect.isEmpty())
        return;

    // Convert pan delta to normalized coordinates, divided by current zoom
    float dx = -delta.x() / itemRect.width() / m_zoom;
    float dy = -delta.y() / itemRect.height() / m_zoom;

    m_centerX += dx;
    m_centerY += dy;

    // Limit center point range to prevent out of bounds
    // float halfView = 0.5f / m_zoom;
    // m_centerX = qBound(halfView, m_centerX, 1.0f - halfView);
    // m_centerY = qBound(halfView, m_centerY, 1.0f - halfView);

    m_renderer->setZoomAndOffset(m_zoom, m_centerX, m_centerY);
    emit zoomChanged();
    update();
}

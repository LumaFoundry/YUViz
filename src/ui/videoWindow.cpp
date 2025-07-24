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

SharedViewProperties* VideoWindow::sharedView() const {
    return m_sharedView;
}

void VideoWindow::setSharedView(SharedViewProperties* view) {
    if (m_sharedView == view)
        return;
    m_sharedView = view;
    if (m_sharedView) {
        connect(m_sharedView, &SharedViewProperties::viewChanged, this, [=]() { update(); });
    }
    emit sharedViewChanged();
}

QSGNode* VideoWindow::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    // qDebug() << "VideoWindow::updatePaintNode called in thread" << QThread::currentThread();

    if (!m_renderer || !m_sharedView) {
        return nullptr;
    }

    // Pass shared values to the renderer
    m_renderer->setZoomAndOffset(m_sharedView->zoom(), m_sharedView->centerX(), m_sharedView->centerY());

    VideoRenderNode* node = static_cast<VideoRenderNode*>(oldNode);
    if (!node) {
        node = new VideoRenderNode(this, m_renderer);
    }
    return node;
}

QPointF VideoWindow::convertToVideoCoordinates(const QPointF& point) const {
    QRectF videoRect = getVideoRect();
    return QPointF(qBound(0.0, (point.x() - videoRect.x()) / videoRect.width(), 1.0),
                   qBound(0.0, (point.y() - videoRect.y()) / videoRect.height(), 1.0));
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
    if (!m_sharedView)
        return;
    const QPointF videoCoord = convertToVideoCoordinates(centerPoint);
    m_sharedView->applyZoom(factor, videoCoord.x(), videoCoord.y());
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
    emit zoomChanged();
    update();
}

void VideoWindow::resetView() {
    if (m_sharedView) {
        m_sharedView->reset();
    }
}

void VideoWindow::zoomToSelection(const QRectF& rect) {
    if (!m_sharedView)
        return;
    m_sharedView->zoomToSelection(
        rect, getVideoRect(), m_sharedView->zoom(), m_sharedView->centerX(), m_sharedView->centerY());
}

QRectF VideoWindow::getVideoRect() const {
    QRectF itemRect = boundingRect();
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
    return videoRect;
}

void VideoWindow::pan(const QPointF& delta) {
    if (!m_sharedView)
        return;
    m_sharedView->applyPan(-delta.x() / width(), -delta.y() / height());
}

void VideoWindow::syncColorSpaceMenu() {
    int colorSpace = 1;
    int colorRange = 1;
    if (m_renderer && m_renderer->getFrameMeta()) {
        colorSpace = static_cast<int>(m_renderer->getFrameMeta()->colorSpace());
        colorRange = static_cast<int>(m_renderer->getFrameMeta()->colorRange());
    }
    int idx = 0;
    QVariant foo;
    if (colorRange == 0)
        colorRange = 2; // UNSPECIFIED
    if (colorSpace == 1 || colorSpace == 2)
        idx = (colorRange == 2 ? 1 : 0);
    else if (colorSpace == 5 || colorSpace == 6)
        idx = (colorRange == 2 ? 3 : 2);
    else if (colorSpace == 10 || colorSpace == 9)
        idx = (colorRange == 2 ? 5 : 4);

    QMetaObject::invokeMethod(this->findChild<QObject*>("videoBridge"),
                              "setColorSpaceIndex",
                              Q_RETURN_ARG(QVariant, foo),
                              Q_ARG(QVariant, idx));
}

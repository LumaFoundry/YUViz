#include "diffWindow.h"
#include <QMouseEvent>
#include <QtMath>
#include <libavutil/pixfmt.h>
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "rendering/diffRenderNode.h"
#include "rendering/diffRenderer.h"

extern "C" {
#include <libavutil/pixdesc.h>
}

DiffWindow::DiffWindow(QQuickItem* parent) :
    QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
}

void DiffWindow::initialize(std::shared_ptr<FrameMeta> metaPtr,
                            std::shared_ptr<FrameQueue> queuePtr1,
                            std::shared_ptr<FrameQueue> queuePtr2) {
    m_frameMeta = metaPtr; // Store the frameMeta for OSD access
    m_frameQueue1 = queuePtr1;
    m_frameQueue2 = queuePtr2;
    m_renderer = new DiffRenderer(this, metaPtr);

    // Set aspect ratio based on actual frame dimensions from frameMeta
    if (metaPtr && metaPtr->yHeight() > 0) {
        m_videoAspectRatio = static_cast<qreal>(metaPtr->yWidth()) / metaPtr->yHeight();
        qDebug() << "[DiffWindow] Setting aspect ratio to" << m_videoAspectRatio << "from frame dimensions"
                 << metaPtr->yWidth() << "x" << metaPtr->yHeight();
    }

    if (window()) {
        update();
    } else {
        connect(
            this,
            &QQuickItem::windowChanged,
            this,
            [=](QQuickWindow* win) {
                qDebug() << "[DiffWindow] window became available, calling update()";
                update();
            },
            Qt::DirectConnection);
    }
}

void DiffWindow::setAspectRatio(int width, int height) {
    if (height > 0) {
        m_videoAspectRatio = static_cast<qreal>(width) / height;
    }
}

qreal DiffWindow::getAspectRatio() const {
    return m_videoAspectRatio;
}

void DiffWindow::uploadFrame(FrameData* frame1, FrameData* frame2) {
    // qDebug() << "DiffWindow::uploadFrame called in thread";

    // Verify that both frames have the same PTS before uploading
    if (frame1 && frame2 && frame1->pts() == frame2->pts()) {
        qDebug() << "DiffWindow: Uploading frames with matching PTS:" << frame1->pts();
        m_renderer->releaseBatch();
        m_renderer->uploadFrame(frame1, frame2);
        emit frameReady();
    } else {
        qWarning() << "DiffWindow: Skipping upload - frames have different PTS values";
        if (frame1 && frame2) {
            qWarning() << "Frame1 PTS:" << frame1->pts() << "Frame2 PTS:" << frame2->pts();
        }
    }
}

void DiffWindow::renderFrame() {
    // qDebug() << "DiffWindow::renderFrame called in thread" << QThread::currentThread();
    update();
}

void DiffWindow::batchIsFull() {
    emit batchUploaded(true);
}

void DiffWindow::batchIsEmpty() {
    emit gpuUploaded(true);
}

void DiffWindow::rendererError() {
    emit errorOccurred();
}

SharedViewProperties* DiffWindow::sharedView() const {
    return m_sharedView;
}

void DiffWindow::setSharedView(SharedViewProperties* view) {
    if (m_sharedView == view)
        return;
    m_sharedView = view;
    if (m_sharedView) {
        connect(m_sharedView, &SharedViewProperties::viewChanged, this, [=]() { update(); }, Qt::DirectConnection);
    }
    emit sharedViewChanged();
}

// Diff configuration methods
void DiffWindow::setDisplayMode(int mode) {
    if (m_displayMode != mode) {
        m_displayMode = mode;
        if (m_renderer) {
            m_renderer->setDiffConfig(m_displayMode, m_diffMultiplier, m_diffMethod);
        }
        emit displayModeChanged();
    }
}

void DiffWindow::setDiffMultiplier(float multiplier) {
    if (m_diffMultiplier != multiplier) {
        m_diffMultiplier = multiplier;
        if (m_renderer) {
            m_renderer->setDiffConfig(m_displayMode, m_diffMultiplier, m_diffMethod);
        }
        emit diffMultiplierChanged();
    }
}

void DiffWindow::setDiffMethod(int method) {
    if (m_diffMethod != method) {
        m_diffMethod = method;
        if (m_renderer) {
            m_renderer->setDiffConfig(m_displayMode, m_diffMultiplier, m_diffMethod);
        }
        emit diffMethodChanged();
    }
}

QVariant DiffWindow::getDiffValue(int x, int y) const {
    if (!m_renderer)
        return QVariant();

    // Get the two frames from renderer
    uint64_t pts1 = m_renderer->getCurrentPts1();
    uint64_t pts2 = m_renderer->getCurrentPts2();
    FrameData* frame1 = m_frameQueue1->getHeadFrame(pts1);
    FrameData* frame2 = m_frameQueue2->getHeadFrame(pts2);
    auto meta = m_renderer->getFrameMeta();

    if (!frame1 || !frame2 || !meta)
        return QVariant();

    int yW = meta->yWidth(), yH = meta->yHeight();
    if (x < 0 || y < 0 || x >= yW || y >= yH)
        return QVariant();

    // Check if frame pointers are valid
    uint8_t* y1Ptr = frame1->yPtr();
    uint8_t* y2Ptr = frame2->yPtr();
    if (!y1Ptr || !y2Ptr)
        return QVariant();

    // Get Y values from both frames with bounds checking
    int y1Val = 0, y2Val = 0;
    try {
        y1Val = y1Ptr[y * yW + x];
        y2Val = y2Ptr[y * yW + x];
    } catch (...) {
        qDebug() << "DiffWindow::getDiffValue - Exception caught when accessing pixel data";
        return QVariant();
    }

    // Calculate simple Y difference (y1 - y2)
    int diff = y1Val - y2Val;

    QVariantList result;
    result << y1Val << y2Val << diff;
    return result;
}

QSGNode* DiffWindow::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    // qDebug() << "DiffWindow::updatePaintNode called in thread" << QThread::currentThread();

    if (!m_renderer || !m_sharedView) {
        return nullptr;
    }

    // Pass shared values to the renderer
    m_renderer->setZoomAndOffset(m_sharedView->zoom(), m_sharedView->centerX(), m_sharedView->centerY());

    DiffRenderNode* node = static_cast<DiffRenderNode*>(oldNode);
    if (!node) {
        node = new DiffRenderNode(this, m_renderer);
    }
    return node;
}

QPointF DiffWindow::convertToVideoCoordinates(const QPointF& point) const {
    QRectF videoRect = getVideoRect();
    return QPointF(qBound(0.0, (point.x() - videoRect.x()) / videoRect.width(), 1.0),
                   qBound(0.0, (point.y() - videoRect.y()) / videoRect.height(), 1.0));
}

qreal DiffWindow::maxZoom() const {
    return m_maxZoom;
}

void DiffWindow::setMaxZoom(qreal zoom) {
    if (qFuzzyCompare(m_maxZoom, zoom) || zoom <= 0)
        return;
    m_maxZoom = zoom;
    emit maxZoomChanged();
}

void DiffWindow::zoomAt(qreal factor, const QPointF& centerPoint) {
    if (!m_sharedView)
        return;
    const QPointF videoCoord = convertToVideoCoordinates(centerPoint);
    m_sharedView->applyZoom(factor, videoCoord.x(), videoCoord.y());
}

void DiffWindow::setSelectionRect(const QRectF& rect) {
    m_selectionRect = rect;
    m_hasSelection = !rect.isNull();
    update();
}

void DiffWindow::clearSelection() {
    m_selectionRect = QRectF();
    m_hasSelection = false;
    m_isSelecting = false;
    emit zoomChanged();
    update();
}

void DiffWindow::resetView() {
    if (m_sharedView) {
        m_sharedView->reset();
    }
}

void DiffWindow::zoomToSelection(const QRectF& rect) {
    if (!m_sharedView)
        return;
    m_sharedView->zoomToSelection(
        rect, getVideoRect(), m_sharedView->zoom(), m_sharedView->centerX(), m_sharedView->centerY());
}

QRectF DiffWindow::getVideoRect() const {
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

void DiffWindow::pan(const QPointF& delta) {
    if (!m_sharedView)
        return;
    m_sharedView->applyPan(-delta.x() / width(), -delta.y() / height());
}

// OSD-related implementations
void DiffWindow::setOsdState(int state) {
    if (m_osdState != state) {
        m_osdState = state;
        emit osdStateChanged();
    }
}

void DiffWindow::toggleOsd() {
    m_osdState = (m_osdState + 1) % 3; // Cycle through 0, 1, 2
    emit osdStateChanged();
}

void DiffWindow::updateFrameInfo(int currentFrame, double currentTimeMs) {
    if (m_currentFrame != currentFrame) {
        m_currentFrame = currentFrame;
        emit currentFrameChanged();
    }
    if (m_currentTimeMs != currentTimeMs) {
        m_currentTimeMs = currentTimeMs;
        emit currentTimeMsChanged();
    }
}

int DiffWindow::currentFrame() const {
    return m_currentFrame;
}

int DiffWindow::totalFrames() const {
    return m_frameMeta ? m_frameMeta->totalFrames() : 0;
}

QString DiffWindow::pixelFormat() const {
    if (!m_frameMeta) {
        return QString("N/A");
    }
    AVPixelFormat fmt = m_frameMeta->format();
    const char* name = av_get_pix_fmt_name(fmt);
    return name ? QString(name) : QString("Unknown");
}

QString DiffWindow::timeBase() const {
    if (!m_frameMeta) {
        return QString("N/A");
    }
    AVRational timeBase = m_frameMeta->timeBase();
    return QString("%1/%2").arg(timeBase.num).arg(timeBase.den);
}

qint64 DiffWindow::duration() const {
    return m_frameMeta ? m_frameMeta->duration() : 0;
}

double DiffWindow::currentTimeMs() const {
    return m_currentTimeMs;
}

QVariantMap DiffWindow::getFrameMeta() const {
    QVariantMap result;
    if (!m_renderer) {
        return result;
    }

    auto frameMeta = m_renderer->getFrameMeta();
    if (!frameMeta) {
        return result;
    }

    result["yWidth"] = frameMeta->yWidth();
    result["yHeight"] = frameMeta->yHeight();
    result["uvWidth"] = frameMeta->uvWidth();
    result["uvHeight"] = frameMeta->uvHeight();
    result["format"] = frameMeta->format();

    return result;
}
#include "videoWindow.h"
#include <QMouseEvent>
#include <QtMath>
#include <libavutil/pixfmt.h>
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "rendering/videoRenderNode.h"
#include "rendering/videoRenderer.h"

extern "C" {
#include <libavutil/pixdesc.h>
}

VideoWindow::VideoWindow(QQuickItem* parent) :
    QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
}

void VideoWindow::initialize(std::shared_ptr<FrameMeta> metaPtr) {
    m_frameMeta = metaPtr; // Store the frameMeta for OSD access
    m_renderer = new VideoRenderer(this, metaPtr);

    // Set aspect ratio based on actual frame dimensions from frameMeta
    if (metaPtr && metaPtr->yHeight() > 0) {
        m_videoAspectRatio = static_cast<qreal>(metaPtr->yWidth()) / metaPtr->yHeight();
        qDebug() << "[VideoWindow] Setting aspect ratio to" << m_videoAspectRatio << "from frame dimensions"
                 << metaPtr->yWidth() << "x" << metaPtr->yHeight();
    }

    if (window()) {
        update();
    } else {
        connect(this, &QQuickItem::windowChanged, this, [=](QQuickWindow* win) {
            qDebug() << "[VideoWindow] window became available, calling update()";
            update();
        });
    }
    emit metadataInitialized();
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
    emit frameReady();
}

void VideoWindow::renderFrame() {
    // qDebug() << "VideoWindow::renderFrame called in thread" << QThread::currentThread();

    // Update frame info
    if (m_renderer && m_frameMeta) {
        FrameData* currentFrame = m_renderer->getCurrentFrame();
        if (currentFrame) {
            int64_t pts = currentFrame->pts();
            AVRational timeBase = m_frameMeta->timeBase();
            double currentTimeMs = pts * av_q2d(timeBase) * 1000.0;
            updateFrameInfo(static_cast<int>(pts), currentTimeMs);
        }
    }

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

// OSD-related implementations
void VideoWindow::setOsdState(int state) {
    if (m_osdState != state) {
        m_osdState = state;
        emit osdStateChanged(m_osdState);
    }
}

void VideoWindow::toggleOsd() {
    m_osdState = (m_osdState + 1) % 3; // Cycle through 0, 1, 2
    emit osdStateChanged(m_osdState);
}

void VideoWindow::updateFrameInfo(int currentFrame, double currentTimeMs) {
    if (m_currentFrame != currentFrame) {
        m_currentFrame = currentFrame;
        emit currentFrameChanged();
    }
    if (m_currentTimeMs != currentTimeMs) {
        m_currentTimeMs = currentTimeMs;
        emit currentTimeMsChanged();
    }
}

int VideoWindow::currentFrame() const {
    return m_currentFrame;
}

QString VideoWindow::pixelFormat() const {
    if (!m_frameMeta) {
        return QString("N/A");
    }
    AVPixelFormat fmt = m_frameMeta->format();
    const char* name = av_get_pix_fmt_name(fmt);
    return name ? QString(name) : QString("Unknown");
}

QString VideoWindow::timeBase() const {
    if (!m_frameMeta) {
        return QString("N/A");
    }
    AVRational timeBase = m_frameMeta->timeBase();
    return QString("%1/%2").arg(timeBase.num).arg(timeBase.den);
}

qint64 VideoWindow::duration() const {
    return m_frameMeta ? m_frameMeta->duration() : 0;
}

double VideoWindow::currentTimeMs() const {
    return m_currentTimeMs;
}

QString VideoWindow::colorSpace() const {
    if (!m_frameMeta) {
        return QString("N/A");
    }
    AVColorSpace colorSpace = m_frameMeta->colorSpace();

    // Convert AVColorSpace enum to readable string
    switch (colorSpace) {
    case AVCOL_SPC_BT709:
        return QString("BT.709");
    case AVCOL_SPC_BT470BG:
        return QString("BT.470BG");
    case AVCOL_SPC_SMPTE170M:
        return QString("SMPTE 170M");
    case AVCOL_SPC_SMPTE240M:
        return QString("SMPTE 240M");
    case AVCOL_SPC_BT2020_NCL:
        return QString("BT.2020 NCL");
    case AVCOL_SPC_BT2020_CL:
        return QString("BT.2020 CL");
    case AVCOL_SPC_SMPTE2085:
        return QString("SMPTE 2085");
    case AVCOL_SPC_CHROMA_DERIVED_NCL:
        return QString("Chroma Derived NCL");
    case AVCOL_SPC_CHROMA_DERIVED_CL:
        return QString("Chroma Derived CL");
    case AVCOL_SPC_ICTCP:
        return QString("ICtCp");
    case AVCOL_SPC_RGB:
        return QString("RGB");
    case AVCOL_SPC_UNSPECIFIED:
        return QString("Unspecified");
    default:
        return QString("Unknown (%1)").arg(static_cast<int>(colorSpace));
    }
}

QString VideoWindow::colorRange() const {
    if (!m_frameMeta) {
        return QString("N/A");
    }

    AVColorRange colorRange = m_frameMeta->colorRange();

    switch (colorRange) {
    case AVCOL_RANGE_MPEG:
        return QString("Limited");
    case AVCOL_RANGE_JPEG:
        return QString("Full");
    default:
        return QString("Unspecified");
    }
}

QString VideoWindow::videoResolution() const {
    if (!m_frameMeta) {
        return QString("N/A");
    }

    int width = m_frameMeta->yWidth();
    int height = m_frameMeta->yHeight();

    return QString("%1x%2").arg(width).arg(height);
}

int VideoWindow::componentDisplayMode() const {
    return m_componentDisplayMode;
}

void VideoWindow::setComponentDisplayMode(int mode) {
    if (m_componentDisplayMode != mode) {
        m_componentDisplayMode = mode;
        if (m_renderer) {
            m_renderer->setComponentDisplayMode(mode);
        }
        emit componentDisplayModeChanged();
    }
}

QVariant VideoWindow::getYUV(int x, int y) const {
    if (!m_renderer)
        return QVariant();
    FrameData* frame = m_renderer->getCurrentFrame();
    auto meta = m_renderer->getFrameMeta();
    if (!frame || !meta)
        return QVariant();
    int yW = meta->yWidth(), yH = meta->yHeight();
    int uvW = meta->uvWidth(), uvH = meta->uvHeight();
    if (x < 0 || y < 0 || x >= yW || y >= yH)
        return QVariant();
    int yVal = frame->yPtr()[y * yW + x];
    int uVal = 0, vVal = 0;
    int ux = 0, uy = 0;
    AVPixelFormat fmt = meta->format();
    switch (fmt) {
    case AV_PIX_FMT_YUV420P:
        ux = x / 2;
        uy = y / 2;
        break;
    case AV_PIX_FMT_YUV422P:
        ux = x / 2;
        uy = y;
        break;
    case AV_PIX_FMT_YUV444P:
        ux = x;
        uy = y;
        break;
    default:
        ux = x / 2;
        uy = y / 2;
        break;
    }
    if (ux >= 0 && uy >= 0 && ux < uvW && uy < uvH) {
        uVal = frame->uPtr()[uy * uvW + ux];
        vVal = frame->vPtr()[uy * uvW + ux];
    }
    QVariantList result;
    result << yVal << uVal << vVal;
    return result;
}

QVariantMap VideoWindow::getFrameMeta() const {
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
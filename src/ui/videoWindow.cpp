#include "videoWindow.h"
#include <QSurface>
#include <QDebug>
#include <QWheelEvent>
#include <QMouseEvent>

VideoWindow::VideoWindow(QWindow* parent, QRhi::Implementation graphicsApi)
    : QWindow(parent)
{
    switch (graphicsApi) {
            case QRhi::Null:
                break;
            case QRhi::OpenGLES2:
                setSurfaceType(QSurface::OpenGLSurface);
                break;
            case QRhi::Vulkan:
                setSurfaceType(QSurface::VulkanSurface);
                break;
            case QRhi::D3D11:
            case QRhi::D3D12:
                setSurfaceType(QSurface::Direct3DSurface);
                break;
            case QRhi::Metal:
                setSurfaceType(QSurface::MetalSurface);
                break;
        }
}

double VideoWindow::zoomFactor() const {
    return m_zoomFactor;
}

QRectF VideoWindow::selectionRect() const {
    return m_selectionRect;
}

void VideoWindow::wheelEvent(QWheelEvent* event) {
    double factor = (event->angleDelta().y() > 0) ? 1.25 : 0.8;
    m_zoomFactor *= factor;
    if (m_zoomFactor < 1.0) m_zoomFactor = 1.0;
    if (m_zoomFactor > 512.0) m_zoomFactor = 512.0;
    qDebug() << "Zoom factor:" << m_zoomFactor;
    requestUpdate();
}

void VideoWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_selecting = true;
        m_selectStart = event->position();
        m_selectEnd = m_selectStart;
        m_selectionRect = QRectF();
        qDebug() << "Start select:" << m_selectStart;
        requestUpdate();
    }
}

void VideoWindow::mouseMoveEvent(QMouseEvent* event) {
    if (m_selecting) {
        m_selectEnd = event->position();
        m_selectionRect = QRectF(m_selectStart, m_selectEnd).normalized();
        qDebug() << "Selecting:" << m_selectionRect;
        requestUpdate();
    }
}

void VideoWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (m_selecting && event->button() == Qt::LeftButton) {
        m_selecting = false;
        m_selectEnd = event->position();
        m_selectionRect = QRectF(m_selectStart, m_selectEnd).normalized();
        qDebug() << "Selection finished:" << m_selectionRect;
        requestUpdate();
    }
}
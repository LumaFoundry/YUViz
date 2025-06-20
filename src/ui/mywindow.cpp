#include "mywindow.h"

MyWindow::MyWindow() {
    setTitle("videoplayer");
    resize(900, 600);
    setVisibility(QWindow::Windowed);
    requestActivate();
}

void MyWindow::wheelEvent(QWheelEvent* event) {
    double factor = (event->angleDelta().y() > 0) ? 1.25 : 0.8;
    m_zoomFactor *= factor;
    if (m_zoomFactor < 1.0) m_zoomFactor = 1.0;
    if (m_zoomFactor > 512.0) m_zoomFactor = 512.0;
    qDebug() << "Zoom factor:" << m_zoomFactor;
    requestUpdate();
}

void MyWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_selecting = true;
        m_selectStart = event->position();
        m_selectEnd = m_selectStart;
        m_selectionRect = QRectF();
        qDebug() << "Start select:" << m_selectStart;
        requestUpdate();
    }
}

void MyWindow::mouseMoveEvent(QMouseEvent* event) {
    if (m_selecting) {
        m_selectEnd = event->position();
        m_selectionRect = QRectF(m_selectStart, m_selectEnd).normalized();
        qDebug() << "Selecting:" << m_selectionRect;
        requestUpdate();
    }
}

void MyWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (m_selecting && event->button() == Qt::LeftButton) {
        m_selecting = false;
        m_selectEnd = event->position();
        m_selectionRect = QRectF(m_selectStart, m_selectEnd).normalized();
        qDebug() << "Selection finished:" << m_selectionRect;
        requestUpdate();
    }
} 
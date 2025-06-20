#pragma once
#include <QWindow>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QRectF>
#include <QPointF>
#include <QDebug>

class MyWindow : public QWindow {
public:
    MyWindow();

    double zoomFactor() const { return m_zoomFactor; }
    QRectF selectionRect() const { return m_selectionRect; }

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    double m_zoomFactor = 1.0;
    bool m_selecting = false;
    QPointF m_selectStart;
    QPointF m_selectEnd;
    QRectF m_selectionRect;
}; 
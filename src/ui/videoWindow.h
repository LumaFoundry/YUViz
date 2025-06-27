#pragma once
#include <QWindow>
#include <QKeyEvent>
#include <rhi/qrhi.h>

class VideoWindow : public QWindow
{
    Q_OBJECT
public:
    explicit VideoWindow(QWindow* parent = nullptr, QRhi::Implementation graphicsApi = QRhi::Null);
    ~VideoWindow() override = default;

    double zoomFactor() const;
    QRectF selectionRect() const;

signals:
    void togglePlayPause();

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Space) {
            emit togglePlayPause();
        }
    }

private:
    double m_zoomFactor = 1.0;
    bool m_selecting = false;
    QPointF m_selectStart;
    QPointF m_selectEnd;
    QRectF m_selectionRect;

    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
};

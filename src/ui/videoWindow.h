#pragma once

#include <QObject>
#include <QWindow>
#include <QPointF>
#include <QRectF>
#include <rhi/qrhi.h>

class VideoWindow : public QObject
{
    Q_OBJECT
public:
    explicit VideoWindow(QObject* parent = nullptr, QRhi::Implementation graphicsApi = QRhi::Null);
    ~VideoWindow();

    QWindow* window() const;

    double zoomFactor() const { return m_zoomFactor; }
    QRectF selectionRect() const { return m_selectionRect; }

    void show();
    void resize(int width, int height);
    void setTitle(const QString& title);

signals:
    void zoomChanged(double factor);
    void selectionFinished(const QRectF& rect);
    void resized();

private:
    QWindow* m_window;
    double m_zoomFactor = 1.0;
    bool m_selecting = false;
    QPointF m_selectStart;
    QPointF m_selectEnd;
    QRectF m_selectionRect;
};

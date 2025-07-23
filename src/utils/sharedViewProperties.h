#pragma once

#include <QObject>
#include <QPointF>
#include <QRectF>

class SharedViewProperties : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewChanged)
    Q_PROPERTY(qreal centerX READ centerX WRITE setCenterX NOTIFY viewChanged)
    Q_PROPERTY(qreal centerY READ centerY WRITE setCenterY NOTIFY viewChanged)
    Q_PROPERTY(bool isZoomed READ isZoomed NOTIFY viewChanged)

  public:
    explicit SharedViewProperties(QObject* parent = nullptr);

    qreal zoom() const;
    qreal centerX() const;
    qreal centerY() const;
    bool isZoomed() const;

  public slots:
    void setZoom(qreal zoom);
    void setCenterX(qreal centerX);
    void setCenterY(qreal centerY);
    void reset();
    void applyPan(qreal dx, qreal dy);
    void applyZoom(qreal factor, qreal videoX, qreal videoY);
    void zoomToSelection(const QRectF& selection,
                         const QRectF& videoRect,
                         qreal currentZoom,
                         qreal currentCenterX,
                         qreal currentCenterY);

  signals:
    void viewChanged();

  private:
    qreal m_zoom = 1.0;
    qreal m_centerX = 0.5;
    qreal m_centerY = 0.5;
    qreal m_maxZoom = 10000.0;
};
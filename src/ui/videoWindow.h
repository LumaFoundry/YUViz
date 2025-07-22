#pragma once

#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QtQml/qqml.h>
#include <memory>
#include "frames/frameData.h"
#include "rendering/videoRenderer.h"

class VideoWindow : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool isZoomed READ isZoomed NOTIFY zoomChanged)
    Q_PROPERTY(qreal getAspectRatio READ getAspectRatio CONSTANT)
    Q_PROPERTY(qreal maxZoom READ maxZoom WRITE setMaxZoom NOTIFY maxZoomChanged)
    Q_PROPERTY(bool showPixelValues READ showPixelValues WRITE setShowPixelValues NOTIFY showPixelValuesChanged)
    Q_PROPERTY(qreal pixelValueThreshold READ pixelValueThreshold WRITE setPixelValueThreshold NOTIFY
                   pixelValueThresholdChanged)
    Q_PROPERTY(qreal zoom READ zoom NOTIFY zoomChanged)
    Q_PROPERTY(qreal centerX READ centerX NOTIFY centerXChanged)
    Q_PROPERTY(qreal centerY READ centerY NOTIFY centerYChanged)

  public:
    explicit VideoWindow(QQuickItem* parent = nullptr);
    void initialize(std::shared_ptr<FrameMeta> metaPtr);
    VideoRenderer* m_renderer = nullptr;
    bool isZoomed() const { return m_isZoomed; }
    void setAspectRatio(int width, int height);
    qreal getAspectRatio() const;
    qreal maxZoom() const;
    void setMaxZoom(qreal zoom);
    bool showPixelValues() const { return m_showPixelValues; }
    void setShowPixelValues(bool show);
    qreal pixelValueThreshold() const { return m_pixelValueThreshold; }
    void setPixelValueThreshold(qreal threshold);
    qreal zoom() const { return m_zoom; }
    qreal centerX() const { return m_centerX; }
    qreal centerY() const { return m_centerY; }

  public slots:
    void uploadFrame(FrameData* frame);
    void renderFrame();
    void setColorParams(AVColorSpace space, AVColorRange range);
    void batchIsFull();
    void batchIsEmpty();
    void rendererError();
    void zoomAt(qreal factor, const QPointF& centerPoint);
    void setSelectionRect(const QRectF& rect);
    void clearSelection();
    void zoomToSelection();
    void resetZoom();
    void pan(const QPointF& delta);
    QVariantList getFrameData() const;
    QVariantMap getFrameMeta() const;

  signals:
    void batchUploaded(bool success);
    void gpuUploaded(bool success);
    void errorOccurred();
    void selectionChanged(const QRectF& rect);
    void zoomChanged();
    void maxZoomChanged();
    void showPixelValuesChanged();
    void pixelValueThresholdChanged();
    void centerXChanged();
    void centerYChanged();
    void frameDataUpdated(QVariantMap frameData);

  protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

  private:
    QRectF m_selectionRect;
    bool m_hasSelection = false;
    QPointF m_selectionStart;
    QPointF m_selectionEnd;
    bool m_isSelecting = false;
    bool m_isZoomed = false;
    bool m_showPixelValues = false;
    qreal m_videoAspectRatio = 16.0 / 9.0;
    qreal m_maxZoom = 10000.0;
    qreal m_pixelValueThreshold = 10.0;
    // Track current zoom and center point
    float m_zoom = 1.0f;
    float m_centerX = 0.5f;
    float m_centerY = 0.5f;

    QPointF convertToVideoCoordinates(const QPointF& point) const;
};
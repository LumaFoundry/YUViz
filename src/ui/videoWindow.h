#pragma once

#include <QQuickItem>
#include <QtQml/qqml.h>
#include <QPointF>
#include <QRectF>
#include <memory>
#include "frames/frameData.h"
#include "rendering/videoRenderer.h"

class VideoWindow : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool isZoomed READ isZoomed NOTIFY zoomChanged)
    Q_PROPERTY(QRectF currentZoomRect READ currentZoomRect NOTIFY zoomChanged)
public:
    explicit VideoWindow(QQuickItem *parent = nullptr);
    void initialize(std::shared_ptr<FrameMeta> metaPtr);
    VideoRenderer *m_renderer = nullptr;
    bool isZoomed() const { return m_isZoomed; }
    QRectF currentZoomRect() const { return m_currentZoomRect; }

public slots:
    void uploadFrame(FrameData* frame);
    void renderFrame();
    void setColorParams(AVColorSpace space, AVColorRange range);
    void releaseBatch();
    void batchIsFull();
    void batchIsEmpty();
    void rendererError();
    void setZoom(qreal zoom);
    void setSelectionRect(const QRectF &rect);
    void clearSelection();
    void zoomToSelection();
    void resetZoom();

signals:
    void batchUploaded(bool success);
    void gpuUploaded(bool success);
    void errorOccurred();
    void selectionChanged(const QRectF &rect);
    void zoomChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF m_selectionRect;
    bool m_hasSelection = false;
    QPointF m_selectionStart;
    QPointF m_selectionEnd;
    bool m_isSelecting = false;
    QRectF m_currentZoomRect; 
    bool m_isZoomed = false;   
};

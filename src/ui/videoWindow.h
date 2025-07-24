#pragma once

#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QtQml/qqml.h>
#include <memory>
#include "frames/frameData.h"
#include "rendering/videoRenderer.h"
#include "utils/sharedViewProperties.h"

class VideoWindow : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal getAspectRatio READ getAspectRatio CONSTANT)
    Q_PROPERTY(qreal maxZoom READ maxZoom WRITE setMaxZoom NOTIFY maxZoomChanged)
    Q_PROPERTY(SharedViewProperties* sharedView READ sharedView WRITE setSharedView NOTIFY sharedViewChanged)

  public:
    explicit VideoWindow(QQuickItem* parent = nullptr);
    SharedViewProperties* sharedView() const;
    void setSharedView(SharedViewProperties* view);
    void initialize(std::shared_ptr<FrameMeta> metaPtr);
    VideoRenderer* m_renderer = nullptr;
    void setAspectRatio(int width, int height);
    qreal getAspectRatio() const;
    qreal maxZoom() const;
    void setMaxZoom(qreal zoom);
    void syncColorSpaceMenu();
    Q_INVOKABLE QVariant getYUV(int x, int y) const;

  public slots:
    void uploadFrame(FrameData* frame);
    void renderFrame();
    void setColorParams(AVColorSpace space, AVColorRange range);
    void batchIsFull();
    void batchIsEmpty();
    void rendererError();
    Q_INVOKABLE void zoomAt(qreal factor, const QPointF& centerPoint);
    void setSelectionRect(const QRectF& rect);
    void clearSelection();
    Q_INVOKABLE void zoomToSelection(const QRectF& rect);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void pan(const QPointF& delta);
    QVariantMap getFrameMeta() const;

  signals:
    void batchUploaded(bool success);
    void gpuUploaded(bool success);
    void errorOccurred();
    void selectionChanged(const QRectF& rect);
    void zoomChanged();
    void maxZoomChanged();
    void sharedViewChanged();
    void frameReady();

  protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

  private:
    QRectF m_selectionRect;
    bool m_hasSelection = false;
    QPointF m_selectionStart;
    QPointF m_selectionEnd;
    bool m_isSelecting = false;
    qreal m_videoAspectRatio = 16.0 / 9.0;
    qreal m_maxZoom = 10000.0;
    SharedViewProperties* m_sharedView = nullptr;

    QRectF getVideoRect() const;
    QPointF convertToVideoCoordinates(const QPointF& point) const;
};
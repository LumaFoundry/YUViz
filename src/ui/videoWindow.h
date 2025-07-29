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
    Q_PROPERTY(int osdState READ osdState WRITE setOsdState NOTIFY osdStateChanged)
    Q_PROPERTY(int currentFrame READ currentFrame NOTIFY currentFrameChanged)
    Q_PROPERTY(int totalFrames READ totalFrames CONSTANT)
    Q_PROPERTY(QString pixelFormat READ pixelFormat CONSTANT)
    Q_PROPERTY(QString timeBase READ timeBase CONSTANT)
    Q_PROPERTY(qint64 duration READ duration CONSTANT)
    Q_PROPERTY(double currentTimeMs READ currentTimeMs NOTIFY currentTimeMsChanged)
    Q_PROPERTY(QString colorSpace READ colorSpace CONSTANT)
    Q_PROPERTY(QString colorRange READ colorRange CONSTANT)

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

    // OSD-related methods
    int osdState() const { return m_osdState; }
    void setOsdState(int state);
    int currentFrame() const;
    int totalFrames() const;
    QString pixelFormat() const;
    QString timeBase() const;
    qint64 duration() const;
    double currentTimeMs() const;
    QString colorSpace() const;
    QString colorRange() const;

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
    void toggleOsd();
    void updateFrameInfo(int currentFrame, double currentTimeMs);

  signals:
    void batchUploaded(bool success);
    void gpuUploaded(bool success);
    void errorOccurred();
    void selectionChanged(const QRectF& rect);
    void zoomChanged();
    void maxZoomChanged();
    void sharedViewChanged();
    void frameReady();
    void osdStateChanged();
    void currentFrameChanged();
    void currentTimeMsChanged();

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

    // OSD-related members
    int m_osdState = 0; // 0: hidden, 1: basic info, 2: detailed info
    std::shared_ptr<FrameMeta> m_frameMeta;
    int m_currentFrame = 0;
    double m_currentTimeMs = 0.0;

    QRectF getVideoRect() const;
    QPointF convertToVideoCoordinates(const QPointF& point) const;
};
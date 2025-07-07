#pragma once

#include <QQuickItem>
#include <QtQml/qqml.h>
#include <memory>
#include "frames/frameData.h"
#include "rendering/videoRenderer.h"

class VideoWindow : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
public:
    explicit VideoWindow(QQuickItem *parent = nullptr);
    void initialize(std::shared_ptr<FrameMeta> metaPtr);
    VideoRenderer *m_renderer = nullptr;

public slots:
    void uploadFrame(FrameData* frame);
    void renderFrame();
    void setColorParams(AVColorSpace space, AVColorRange range);
    void releaseBatch();
    void batchIsFull();
    void batchIsEmpty();
    void rendererError();
    void setZoom(qreal zoom);

signals:
    void batchUploaded(bool success);
    void gpuUploaded(bool success);
    void errorOccurred();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;



};

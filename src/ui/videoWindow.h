#pragma once

#include <QQuickItem>
#include <memory>
#include "frames/frameData.h"
#include "rendering/videoRenderer.h"

class VideoWindow : public QQuickItem {
    Q_OBJECT
public:
    explicit VideoWindow(QQuickItem *parent = nullptr);
    void initialize(std::shared_ptr<FrameMeta> metaPtr);
    VideoRenderer *m_renderer = nullptr;

public slots:
    void uploadFrame(FrameData* frame);
    void renderFrame();
    void setColorParams(AVColorSpace space, AVColorRange range);
    void batchIsFull();
    void batchIsEmpty();
    void rendererError();

signals:
    void batchUploaded(bool success);
    void gpuUploaded(bool success);
    void errorOccurred();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;



};

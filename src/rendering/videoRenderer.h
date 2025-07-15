#pragma once

#include <QRectF>
#include <memory>
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "rhi/qrhi.h"

class VideoRenderer : public QObject {
    Q_OBJECT
  public:
    VideoRenderer(QObject* parent, std::shared_ptr<FrameMeta> metaPtr);
    ~VideoRenderer();

    void initialize(QRhi* rhi, QRhiRenderPassDescriptor* rp);
    void setColorParams(AVColorSpace space, AVColorRange range);
    void uploadFrame(FrameData* frame);
    void renderFrame(QRhiCommandBuffer* cb, const QRect& viewport, QRhiRenderTarget* rt);
    void releaseBatch();

  signals:
    void batchIsFull();
    void batchIsEmpty();
    void rendererError();

  public slots:
    void setZoomAndOffset(const float zoom, const float centerX, const float centerY);

  private:
    std::shared_ptr<FrameMeta> m_metaPtr;
    QRhi* m_rhi = nullptr;
    float m_zoom = 1.0f;
    float m_centerX = 0.5f;
    float m_centerY = 0.5f;
    std::unique_ptr<QRhiTexture> m_yTex;
    std::unique_ptr<QRhiTexture> m_uTex;
    std::unique_ptr<QRhiTexture> m_vTex;
    std::unique_ptr<QRhiBuffer> m_colorParams;
    std::unique_ptr<QRhiBuffer> m_resizeParams;
    std::unique_ptr<QRhiGraphicsPipeline> m_pip;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_resourceBindings;
    std::unique_ptr<QRhiBuffer> m_vbuf;
    float m_windowAspect = 0;

    QRhiResourceUpdateBatch* m_initBatch = nullptr;
    QRhiResourceUpdateBatch* m_frameBatch = nullptr;
    QRhiResourceUpdateBatch* m_colorParamsBatch = nullptr;
    QRhiResourceUpdateBatch* m_resizeParamsBatch = nullptr;

    QByteArray loadShaderSource(const QString& path);
};

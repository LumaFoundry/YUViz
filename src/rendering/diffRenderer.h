#pragma once

#include <QRectF>
#include <memory>
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "rhi/qrhi.h"

extern "C" {
#include <libavutil/rational.h>
}

class DiffRenderer : public QObject {
    Q_OBJECT
  public:
    DiffRenderer(QObject* parent, std::shared_ptr<FrameMeta> metaPtr);
    ~DiffRenderer();

    void initialize(QRhi* rhi, QRhiRenderPassDescriptor* rp);
    void setDiffConfig(int displayMode, float diffMultiplier, int diffMethod);
    void uploadFrame(FrameData* frame1, FrameData* frame2);
    void renderFrame(QRhiCommandBuffer* cb, const QRect& viewport, QRhiRenderTarget* rt);
    void releaseBatch();

  signals:
    void batchIsFull();
    void batchIsEmpty();
    void rendererError();

  public slots:
    void setZoomAndOffset(const float zoom, const float centerX, const float centerY);

  public:
    std::shared_ptr<FrameMeta> getFrameMeta() const { return m_metaPtr; }
    uint64_t getCurrentPts1() const { return m_currentPts1; }
    uint64_t getCurrentPts2() const { return m_currentPts2; }

  private:
    std::shared_ptr<FrameMeta> m_metaPtr;
    uint64_t m_currentPts1 = 0;
    uint64_t m_currentPts2 = 0;
    QRhi* m_rhi = nullptr;
    float m_zoom = 1.0f;
    float m_centerX = 0.5f;
    float m_centerY = 0.5f;
    std::unique_ptr<QRhiTexture> m_yTex1;
    std::unique_ptr<QRhiTexture> m_yTex2;
    std::unique_ptr<QRhiBuffer> m_diffConfig;
    std::unique_ptr<QRhiBuffer> m_resizeParams;
    std::unique_ptr<QRhiGraphicsPipeline> m_pip;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_resourceBindings;
    std::unique_ptr<QRhiBuffer> m_vbuf;
    float m_windowAspect = 0;

    QRhiResourceUpdateBatch* m_initBatch = nullptr;
    QRhiResourceUpdateBatch* m_diffConfigBatch = nullptr;
    QRhiResourceUpdateBatch* m_frameBatch = nullptr;
    QRhiResourceUpdateBatch* m_resizeParamsBatch = nullptr;

    QByteArray loadShaderSource(const QString& path);
};

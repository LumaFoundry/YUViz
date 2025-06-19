#pragma once

#include <memory>
#include <QSize>
#include <QWindow>
#include "rhi/qrhi.h"
#include "frames/frameData.h"
#include "frames/frameMeta.h"

class VideoRenderer : public QObject {
    Q_OBJECT
public:
    VideoRenderer(QWindow* window,
                  std::shared_ptr<FrameMeta> metaPtr);
    ~VideoRenderer();

    void initialize(QRhi::Implementation impl);
    void setColorParams(AVColorSpace space, AVColorRange range);

public slots:
    void uploadFrame(FrameData* frame);
    void renderFrame();

signals:
    void batchUploaded();
    void gpuUploaded();
    void errorOccurred();


private:
    VideoRenderer(const VideoRenderer&) = delete;
    VideoRenderer& operator=(const VideoRenderer&) = delete;

    QWindow* m_window = nullptr;
    std::shared_ptr<FrameMeta> m_metaPtr;
    std::unique_ptr<QRhi> m_rhi;
    std::unique_ptr<QRhiSwapChain> m_swapChain;
    std::unique_ptr<QRhiRenderPassDescriptor> m_renderPassDesc;
    std::unique_ptr<QRhiTexture> m_yTex;
    std::unique_ptr<QRhiTexture> m_uTex;
    std::unique_ptr<QRhiTexture> m_vTex;
    QRhiResourceUpdateBatch* m_initBatch = nullptr;
    QRhiResourceUpdateBatch* m_frameBatch = nullptr;
    QRhiResourceUpdateBatch* m_colorParamsBatch = nullptr;
    std::unique_ptr<QRhiBuffer> m_colorParams;
    std::unique_ptr<QRhiGraphicsPipeline> m_pip;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_resourceBindings;
    std::unique_ptr<QRhiBuffer> m_vbuf;

    QByteArray loadShaderSource(const QString &path);
};

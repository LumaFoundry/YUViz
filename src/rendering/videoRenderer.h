#pragma once
#include <rhi/qrhi.h>
#include <QSize>
#include "../frames/frameData.h"

class VideoRenderer {
public:
    struct Vertex {
        QVector2D pos;
        QVector2D texCoord;
    };

    VideoRenderer();
    ~VideoRenderer();

    // Set up pipeline, shaders, buffers
    void initialize(QRhi *rhiBackend,
                    QRhiSwapChain *swapChain,
                    QRhiRenderPassDescriptor *rpDesc,
                    const QString &vertexShaderPath = ":/shaders/vertex.qsb",
                    const QString &fragmentShaderPath = ":/shaders/fragment.qsb");

    // Copy frame data to GPU
    void uploadFrame(const FrameData& frame);

    // Issue draw command
    void render(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rp);

    QRhiTexture *createTexture();

private:
    QRhi* rhi = nullptr;

    QRhiBuffer* vertexBuffer = nullptr;
    QRhiTexture* yTex = nullptr;
    QRhiTexture* uTex = nullptr;
    QRhiTexture* vTex = nullptr;

    QRhiSampler* sampler = nullptr;
    QRhiShaderResourceBindings* bindings = nullptr;
    QRhiGraphicsPipeline* pipeline = nullptr;

    QSize currentSize; // for avoiding redundant texture reallocation

    VideoRenderer(const VideoRenderer&) = delete;
    VideoRenderer& operator=(const VideoRenderer&) = delete;
};

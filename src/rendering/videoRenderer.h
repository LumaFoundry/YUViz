#pragma once
#include "rhi/qrhi.h"
#include <QSize>
#include "frames/frameData.h"

class VideoRenderer : public QObject {
    Q_OBJECT
public:
    struct Vertex {
        QVector2D pos;
        QVector2D texCoord;
    };

    VideoRenderer(QObject *parent = nullptr);
    ~VideoRenderer();

    // Set up pipeline, shaders, buffers
    void initialize(QRhi *rhiBackend,
                    QRhiSwapChain *swapChain,
                    QRhiRenderPassDescriptor *rpDesc,
                    const QString &vertexShaderPath = ":/shaders/vertex.qsb",
                    const QString &fragmentShaderPath = ":/shaders/fragment.qsb");


    // Issue draw command
    void render(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rp);

    QRhiTexture *createTexture();

public slots:
    void uploadFrame(FrameData* frame);
    void renderFrame();

signals:
    void frameUploaded(bool success);
    void errorOccurred();

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

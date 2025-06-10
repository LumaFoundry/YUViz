#include "videoRenderer.h"

VideoRenderer::VideoRenderer(): 
    rhi(nullptr),
    vertexBuffer(nullptr),
    yTex(nullptr),
    uTex(nullptr),
    vTex(nullptr),
    sampler(nullptr),
    bindings(nullptr),
    pipeline(nullptr) {}

VideoRenderer::~VideoRenderer() {

    if (vertexBuffer){
        delete vertexBuffer;
        vertexBuffer = nullptr;
    }

    if (yTex) {
        delete yTex;
        yTex = nullptr;
    }

    if (uTex) {
        delete uTex;
        uTex = nullptr;
    }

    if (vTex) {
        delete vTex;
        vTex = nullptr;
    }

    if (sampler) {
        delete sampler;
        sampler = nullptr;
    }

    if (bindings) {
        delete bindings;
        bindings = nullptr;
    }

    if (pipeline) {
        delete pipeline;
        pipeline = nullptr;
    }

}


void VideoRenderer::initialize(QRhi *rhiBackend,
                               QRhiSwapChain *swapChain,
                               QRhiRenderPassDescriptor *rpDesc,
                               const QString &vertexShaderPath,
                               const QString &fragmentShaderPath) {

    // Safeguard against multiple initializations
    if (pipeline)
        return;

    rhi = rhiBackend;

    Vertex vertices[] = {
        {QVector2D(-1.0f, -1.0f), QVector2D(0.0f, 1.0f)},
        {QVector2D( 1.0f, -1.0f), QVector2D(1.0f, 1.0f)},
        {QVector2D(-1.0f,  1.0f), QVector2D(0.0f, 0.0f)},
        {QVector2D( 1.0f,  1.0f), QVector2D(1.0f, 0.0f)}
    };

    // Create vertex buffer
    vertexBuffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices));
    
    if (!vertexBuffer->create()) {
        qWarning("Failed to create vertex buffer");
        return;
    }

    // Upload vertex data
    QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
    batch->uploadStaticBuffer(vertexBuffer, vertices);

    // Create textures for YUV components

    // Create sampler

    // Create shader resource bindings

    // Create graphics pipeline
    


}
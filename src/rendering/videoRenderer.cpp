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
    // TODO: Check QSize not sure
    yTex = rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1), 1, QRhiTexture::RenderTarget | QRhiTexture::Sampled);
    uTex = rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1), 1, QRhiTexture::RenderTarget | QRhiTexture::Sampled);
    vTex = rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1), 1, QRhiTexture::RenderTarget | QRhiTexture::Sampled);

    // Create sampler
    // TODO: What filters and wrap modes to use? Maybe modularise?
    sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

    // Create shader resource bindings
    shaderBindings = rhi->newShaderResourceBindings();
    shaderBindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, vertexBuffer),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, yTex, sampler),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, uTex, sampler),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, vTex, sampler)
    });

    // Create graphics pipeline
    pipeline = rhi->newGraphicsPipeline();
    pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vertexShaderPath},
        {QRhiShaderStage::Fragment, fragmentShaderPath}
    });
    pipeline->setRenderPassDescriptor(rpDesc);
    pipeline->setShaderResourceBindings(shaderBindings);
}


void VideoRenderer::uploadFrame(const FrameData& frame) {

    if (!yTex || !uTex || !vTex) {
        qWarning("Textures not initialized");
        return;
    }

    // Upload YUV data to textures
    QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
    
    batch->uploadTexture(yTex, frame.yData, frame.ySize);
    batch->uploadTexture(uTex, frame.uData, frame.uSize);
    batch->uploadTexture(vTex, frame.vData, frame.vSize);

    rhi->submitResourceUpdateBatch(batch);
}


void VideoRenderer::render(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rp) {

    if (!pipeline || !bindings || !vertexBuffer) {
        qWarning("Pipeline or bindings not initialized");
        return;
    }

    cb->beginPass(rp, Qt::black);
    
    cb->setGraphicsPipeline(pipeline);
    cb->setShaderResources(bindings);
    cb->setVertexInput(0, vertexBuffer);

    // Draw two triangles forming a rectangle
    cb->draw(6);

    cb->endPass();

    emit frameRendered();
}


void VideoRenderer::renderFrame(FrameData* frame) {
    if (!frame) {
        qWarning("Received null frame");
        return;
    }

    uploadFrame(*frame);
    
    QRhiCommandBuffer* cb = rhi->currentCommandBuffer();
    QRhiRenderPassDescriptor* rp = rhi->currentRenderPassDescriptor();

    render(cb, rp);

    // DO NOT delete frame here, it is managed by the caller
    
    emit frameRendered();
}


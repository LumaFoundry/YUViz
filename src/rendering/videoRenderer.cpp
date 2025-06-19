#include <QFile>
#include "videoRenderer.h"

VideoRenderer::VideoRenderer(QWindow* window,
                             std::shared_ptr<FrameMeta> metaPtr): 
    QObject(window),
    m_window(window),
    m_metaPtr(metaPtr) {}

VideoRenderer::~VideoRenderer() = default;


void VideoRenderer::initialize(QRhi::Implementation impl) 
{
    switch (impl)
    {
    case QRhi::Null:
        break;
    case QRhi::Vulkan:
        break;
    case QRhi::OpenGLES2:
        break;
    case QRhi::D3D11:
        break;
    case QRhi::D3D12:
        break;
    case QRhi::Metal:
        QRhiMetalInitParams params;
        m_rhi.reset(QRhi::create(QRhi::Metal, &params));
        break;
    }

    if (m_rhi) {
        qDebug() << m_rhi->backendName() << m_rhi->driverInfo();
    }
    else {
        qWarning() << "Failed to initialize RHI";
        emit errorOccurred();
        return;
    }

    m_swapChain.reset(m_rhi->newSwapChain());
    m_swapChain->setWindow(m_window);
    m_renderPassDesc.reset(m_swapChain->newCompatibleRenderPassDescriptor());

    if (!m_renderPassDesc) {
        qWarning() << "Failed to create render pass descriptor";
        emit errorOccurred();
        return;
    }
    
    m_swapChain->setRenderPassDescriptor(m_renderPassDesc.get());
    
    if (!m_swapChain->createOrResize()) {
        qWarning() << "Failed to create swap chain";
        emit errorOccurred();
        return;
    }

    QObject::connect(m_window, &QWindow::widthChanged, this, [this](int){ m_swapChain->createOrResize(); });
    QObject::connect(m_window, &QWindow::heightChanged, this, [this](int){ m_swapChain->createOrResize(); });

    m_yTex.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_metaPtr->yWidth(), m_metaPtr->yHeight())));
    m_uTex.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_metaPtr->uvWidth(), m_metaPtr->uvHeight())));
    m_vTex.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_metaPtr->uvWidth(), m_metaPtr->uvHeight())));

    if (!m_yTex->create() || !m_uTex->create() || !m_vTex->create()) {
        qWarning() << "Failed to create YUV textures";
        emit errorOccurred();
        return;
    }

    QByteArray vsQsb = loadShaderSource("../src/shaders/vertex.qsb");
    QByteArray fsQsb = loadShaderSource("../src/shaders/fragment.qsb");

    if (vsQsb.isEmpty() || fsQsb.isEmpty()) {
        qWarning() << "Failed to open shader file";
        emit errorOccurred();
        return;
    }

    QShader vs = QShader::fromSerialized(vsQsb);
    QShader fs = QShader::fromSerialized(fsQsb);

    m_pip.reset(m_rhi->newGraphicsPipeline());
    m_pip->setShaderStages({
        { QRhiShaderStage::Vertex,   vs },
        { QRhiShaderStage::Fragment, fs }
    });

    QRhiVertexInputLayout vil;
    vil.setBindings({ { sizeof(float)*4 } });
    vil.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, sizeof(float)*2 }
    });
    m_pip->setVertexInputLayout(vil);

    m_pip->setCullMode(QRhiGraphicsPipeline::None);
    m_pip->setTargetBlends({ QRhiGraphicsPipeline::TargetBlend() });
    m_pip->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_pip->setSampleCount(m_swapChain->sampleCount());
    m_pip->setDepthTest(false);
    m_pip->setDepthWrite(false);
    m_pip->setRenderPassDescriptor(m_swapChain->renderPassDescriptor());
    m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear,
                                      QRhiSampler::Linear,
                                      QRhiSampler::None,
                                      QRhiSampler::Repeat,
                                      QRhiSampler::Repeat));
    
    if (!m_sampler->create()) {
        qWarning() << "Failed to create sampler";
        emit errorOccurred();
        return;
    }

    m_resourceBindings.reset(m_rhi->newShaderResourceBindings());
    m_resourceBindings->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_yTex.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, m_uTex.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, m_vTex.get(), m_sampler.get())
    });
    
    if (!m_resourceBindings->create()) {
        qWarning() << "Failed to create shader resource bindings";
        emit errorOccurred();
        return;
    }
    
    m_pip->setShaderResourceBindings(m_resourceBindings.get());
    
    if (!m_pip->create()) {
        qWarning() << "Failed to create graphics pipeline";
        emit errorOccurred();
        return;
    }

    struct V { float x,y,u,v; };
    V verts[4] = {
        {-1,-1, 0, 1},
        {-1, 1, 0, 0},
        { 1,-1, 1, 1},
        { 1, 1, 1, 0}
    };
    m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Immutable,
                                  QRhiBuffer::VertexBuffer,
                                  sizeof(verts)));
    
    if (!m_vbuf->create()) {
        qWarning() << "Failed to create vertex buffer";
        emit errorOccurred();
        return;
    }
    {
        m_initBatch = m_rhi->nextResourceUpdateBatch();
        m_initBatch->uploadStaticBuffer(m_vbuf.get(), 0, sizeof(verts), verts);
    }
}

QByteArray VideoRenderer::loadShaderSource(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

void VideoRenderer::uploadFrame(FrameData* frame) {
    m_batch = m_rhi->nextResourceUpdateBatch();

    QRhiTextureUploadDescription yDesc;
    {
        QRhiTextureSubresourceUploadDescription sd(frame->yPtr(), m_metaPtr->yWidth() * m_metaPtr->yHeight());
        sd.setDataStride(m_metaPtr->yWidth());
        yDesc.setEntries({ {0, 0, sd} });
    }
    m_batch->uploadTexture(m_yTex.get(), yDesc);

    QRhiTextureUploadDescription uDesc;
    {
        QRhiTextureSubresourceUploadDescription sd(frame->uPtr(), m_metaPtr->uvWidth() * m_metaPtr->uvHeight());
        sd.setDataStride(m_metaPtr->uvWidth());
        uDesc.setEntries({ {0, 0, sd} });
    }
    m_batch->uploadTexture(m_uTex.get(), uDesc);

    QRhiTextureUploadDescription vDesc;
    {
        QRhiTextureSubresourceUploadDescription sd(frame->vPtr(), m_metaPtr->uvWidth() * m_metaPtr->uvHeight());
        sd.setDataStride(m_metaPtr->uvWidth());
        vDesc.setEntries({ {0, 0, sd} });
    }
    m_batch->uploadTexture(m_vTex.get(), vDesc);

    emit frameUploaded();
}


void VideoRenderer::renderFrame() {
    m_rhi->beginFrame(m_swapChain.get());
    QRhiCommandBuffer* cb = m_swapChain->currentFrameCommandBuffer();

    if (m_initBatch) {
        cb->resourceUpdate(m_initBatch);
        m_initBatch = nullptr;
    }

    if (m_batch) {
        cb->resourceUpdate(m_batch);
        m_batch = nullptr;
    }

    cb->beginPass(m_swapChain->currentFrameRenderTarget(),
                  QColor(0, 0, 0, 255),
                  {1.0f, 0});
    cb->setGraphicsPipeline(m_pip.get());

    const qreal dpr = m_window->devicePixelRatio();
    const int winWidth = m_window->width();
    const int winHeight = m_window->height();

    const int physWidth = winWidth * dpr;
    const int physHeight = winHeight * dpr;

    const float winAspect = float(winWidth) / winHeight;
    const float vidAspect = float(m_metaPtr->yWidth()) / m_metaPtr->yHeight();

    int vpX = 0, vpY = 0, vpW = physWidth, vpH = physHeight;
    if (winAspect > vidAspect) {
        vpH = physHeight;
        vpW = int(vidAspect * vpH + 0.5f);
        vpX = (physWidth - vpW) / 2;
    } else if (vidAspect > winAspect) {
        vpW = physWidth;
        vpH = int(vpW / vidAspect + 0.5f);
        vpY = (physHeight - vpH) / 2;
    }

    cb->setViewport(QRhiViewport(vpX, vpY, vpW, vpH));

    QRhiCommandBuffer::VertexInput vi(m_vbuf.get(), 0);
    cb->setVertexInput(0,1,&vi);
    cb->setShaderResources();
    cb->draw(4);
    cb->endPass();
    m_rhi->endFrame(m_swapChain.get());
}

#include <QFile>
#include "videoRenderer.h"

VideoRenderer::VideoRenderer(QWindow* window,
                             int yWidth,
                             int yHeight,
                             int uvWidth,
                             int uvHeight): 
    QObject(window),
    m_window(window),
    m_yWidth(yWidth),
    m_yHeight(yHeight),
    m_uvWidth(uvWidth),
    m_uvHeight(uvHeight) {}

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
    QRhiRenderPassDescriptor* rpDesc = m_swapChain->newCompatibleRenderPassDescriptor();
    m_swapChain->setRenderPassDescriptor(rpDesc);
    m_swapChain->createOrResize();

    QObject::connect(m_window, &QWindow::widthChanged, this, [this](int){ m_swapChain->createOrResize(); });
    QObject::connect(m_window, &QWindow::heightChanged, this, [this](int){ m_swapChain->createOrResize(); });

    m_yTex.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_yWidth, m_yHeight)));
    m_uTex.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_uvWidth, m_uvHeight)));
    m_vTex.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_uvWidth, m_uvHeight)));

    bool okY = m_yTex->create();
    bool okU = m_uTex->create();
    bool okV = m_vTex->create();

    if (!(okY && okU && okV)) {
        qWarning() << "Failed to create YUV textures";
        emit errorOccurred();
        return;
    }

    QByteArray vsQsb = loadShaderSource("../shaders/vertex.qsb");
    QByteArray fsQsb = loadShaderSource("../shaders/fragment.qsb");

    if (vsQsb.isEmpty() || fsQsb.isEmpty()) {
        qWarning() << "Missing .qsb shader files";
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
    m_sampler->create();
    m_resourceBindings.reset(m_rhi->newShaderResourceBindings());
    m_resourceBindings->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_yTex.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, m_uTex.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, m_vTex.get(), m_sampler.get())
    });
    m_resourceBindings->create();
    m_pip->setShaderResourceBindings(m_resourceBindings.get());
    m_pip->create();

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
    m_vbuf->create();
    {
        QRhiResourceUpdateBatch* initBatch = m_rhi->nextResourceUpdateBatch();
        initBatch->uploadStaticBuffer(m_vbuf.get(), 0, sizeof(verts), verts);
        m_rhi->beginFrame(m_swapChain.get());
        QRhiCommandBuffer* cb = m_swapChain->currentFrameCommandBuffer();
        cb->resourceUpdate(initBatch);
        m_rhi->endFrame(m_swapChain.get());
    }
}

static QByteArray loadShaderSource(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open shader file" << path;
        return {};
    }
    return f.readAll();
}

void VideoRenderer::uploadFrame(FrameData& frame) {
    m_batch = m_rhi->nextResourceUpdateBatch();

    QRhiTextureUploadDescription yDesc;
    {
        QRhiTextureSubresourceUploadDescription sd(frame.yPtr(), m_yWidth * m_yHeight);
        sd.setDataStride(m_yWidth);
        yDesc.setEntries({ {0, 0, sd} });
    }
    m_batch->uploadTexture(m_yTex.get(), yDesc);

    QRhiTextureUploadDescription uDesc;
    {
        QRhiTextureSubresourceUploadDescription sd(frame.uPtr(), m_uvWidth * m_uvHeight);
        sd.setDataStride(m_uvWidth);
        uDesc.setEntries({ {0, 0, sd} });
    }
    m_batch->uploadTexture(m_uTex.get(), uDesc);

    QRhiTextureUploadDescription vDesc;
    {
        QRhiTextureSubresourceUploadDescription sd(frame.vPtr(), m_uvWidth * m_uvHeight);
        sd.setDataStride(m_uvWidth);
        vDesc.setEntries({ {0, 0, sd} });
    }
    m_batch->uploadTexture(m_vTex.get(), vDesc);

    emit frameUpLoaded();
}


void VideoRenderer::renderFrame() {
    m_rhi->beginFrame(m_swapChain.get());
    QRhiCommandBuffer* cb = m_swapChain->currentFrameCommandBuffer();

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
    const float vidAspect = float(m_yWidth) / m_yHeight;

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

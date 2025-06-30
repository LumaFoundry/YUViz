#include <QFile>
#include "videoRenderer.h"

VideoRenderer::VideoRenderer(QObject* parent, 
                             std::shared_ptr<FrameMeta> metaPtr): 
    QObject(parent),
    m_metaPtr(metaPtr) {}

VideoRenderer::~VideoRenderer() = default;

void VideoRenderer::initialize(QRhi *rhi, QRhiRenderPassDescriptor *rp) {
    m_rhi = rhi;

    // Create YUV textures
    m_yTex.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_metaPtr->yWidth(), m_metaPtr->yHeight())));
    m_uTex.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_metaPtr->uvWidth(), m_metaPtr->uvHeight())));
    m_vTex.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_metaPtr->uvWidth(), m_metaPtr->uvHeight())));
    if (!m_yTex->create() || !m_uTex->create() || !m_vTex->create()) {
        qWarning() << "Failed to create YUV textures";
        emit rendererError();
        return;
    }

    // Uniform buffer
    m_colorParams.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(int)*4));
    if (!m_colorParams->create()) {
        qWarning() << "Failed to create color parameters uniform buffer";
        emit rendererError();
        return;
    }

    setColorParams(m_metaPtr->colorSpace(), m_metaPtr->colorRange());

    // Load shaders
    Q_INIT_RESOURCE(videoplayer_shaders);
    QByteArray vsQsb = loadShaderSource(":/shaders/vertex.vert.qsb");
    QByteArray fsQsb = loadShaderSource(":/shaders/fragment.frag.qsb");

    if (vsQsb.isEmpty() || fsQsb.isEmpty()) {
        qWarning() << "Failed to open shader file";
        emit rendererError();
        return;
    }

    QShader vs = QShader::fromSerialized(vsQsb);
    QShader fs = QShader::fromSerialized(fsQsb);

    // Graphics pipeline
    m_pip.reset(m_rhi->newGraphicsPipeline());
    m_pip->setShaderStages({ { QRhiShaderStage::Vertex, vs },
                             { QRhiShaderStage::Fragment, fs } });
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
    m_pip->setSampleCount(1);                        // single-sample target
    m_pip->setDepthTest(false);
    m_pip->setDepthWrite(false);
    m_pip->setRenderPassDescriptor(rp);
    m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                      QRhiSampler::None, QRhiSampler::Repeat, QRhiSampler::Repeat));
    if (!m_sampler->create()) {
        qWarning() << "Failed to create sampler";
        emit rendererError();
        return;
    }

    m_resourceBindings.reset(m_rhi->newShaderResourceBindings());
    m_resourceBindings->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_yTex.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, m_uTex.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, m_vTex.get(), m_sampler.get()),
        QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, m_colorParams.get())
    });
    if (!m_resourceBindings->create()) {
        qWarning() << "Failed to create shader resource bindings";
        emit rendererError();
        return;
    }
    
    m_pip->setShaderResourceBindings(m_resourceBindings.get());
    if (!m_pip->create()) {
        qWarning() << "Failed to create graphics pipeline";
        emit rendererError();
        return;
    }

    // Vertex buffer
    struct V { float x,y,u,v; };
    V verts[4] = {
        {-1,-1,0,1}, {-1,1,0,0},
        {1,-1,1,1}, {1,1,1,0}
    };
    m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(verts)));
    if (!m_vbuf->create()) {
        qWarning() << "Failed to create vertex buffer";
        emit rendererError();
        return;
    }

    m_initBatch = m_rhi->nextResourceUpdateBatch();
    m_initBatch->uploadStaticBuffer(m_vbuf.get(), 0, sizeof(verts), verts);
}

QByteArray VideoRenderer::loadShaderSource(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

void VideoRenderer::setColorParams(AVColorSpace space, AVColorRange range) {
    struct ColorParams { int colorSpace, colorRange, padding[2]; };
    ColorParams cp = { space, range, {0,0} };
    m_colorParamsBatch = m_rhi->nextResourceUpdateBatch();
    m_colorParamsBatch->updateDynamicBuffer(m_colorParams.get(), 0, sizeof(cp), &cp);
}


void VideoRenderer::uploadFrame(FrameData* frame) {
    m_frameBatch = m_rhi->nextResourceUpdateBatch();

    QRhiTextureUploadDescription yDesc;
    {
        QRhiTextureSubresourceUploadDescription sd(frame->yPtr(), m_metaPtr->yWidth() * m_metaPtr->yHeight());
        sd.setDataStride(m_metaPtr->yWidth());
        yDesc.setEntries({ {0, 0, sd} });
    }
    m_frameBatch->uploadTexture(m_yTex.get(), yDesc);

    QRhiTextureUploadDescription uDesc;
    {
        QRhiTextureSubresourceUploadDescription sd(frame->uPtr(), m_metaPtr->uvWidth() * m_metaPtr->uvHeight());
        sd.setDataStride(m_metaPtr->uvWidth());
        uDesc.setEntries({ {0, 0, sd} });
    }
    m_frameBatch->uploadTexture(m_uTex.get(), uDesc);

    QRhiTextureUploadDescription vDesc;
    {
        QRhiTextureSubresourceUploadDescription sd(frame->vPtr(), m_metaPtr->uvWidth() * m_metaPtr->uvHeight());
        sd.setDataStride(m_metaPtr->uvWidth());
        vDesc.setEntries({ {0, 0, sd} });
    }
    m_frameBatch->uploadTexture(m_vTex.get(), vDesc);

    emit batchIsFull();    
}

void VideoRenderer::renderFrame(QRhiCommandBuffer *cb, const QRect &viewport, QRhiRenderTarget *rt) {
    if (m_initBatch) {
        cb->resourceUpdate(m_initBatch);
        m_initBatch = nullptr;
    }
    if (m_colorParamsBatch) {
        cb->resourceUpdate(m_colorParamsBatch);
        m_colorParamsBatch = nullptr;
    }
    if (m_frameBatch) {
        cb->resourceUpdate(m_frameBatch);
        m_frameBatch = nullptr;
    }
    emit batchIsEmpty();

    // Preserve aspect ratio by computing a letterboxed viewport
    float windowAspect = float(viewport.width()) / viewport.height();
    float videoAspect = float(m_metaPtr->yWidth()) / m_metaPtr->yHeight();
    int vpX, vpY, vpW, vpH;
    if (windowAspect > videoAspect) {
        // window is wider than video: height fits, width letterboxed
        vpH = viewport.height();
        vpW = int(videoAspect * vpH + 0.5f);
        vpX = viewport.x() + (viewport.width() - vpW) / 2;
        vpY = viewport.y();
    } else {
        // window is taller than video: width fits, height letterboxed
        vpW = viewport.width();
        vpH = int(vpW / videoAspect + 0.5f);
        vpX = viewport.x();
        vpY = viewport.y() + (viewport.height() - vpH) / 2;
    }
    cb->setViewport(QRhiViewport(vpX, vpY, vpW, vpH));

    // Draw
    cb->setGraphicsPipeline(m_pip.get());
    QRhiCommandBuffer::VertexInput vi(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vi);
    cb->setShaderResources();
    
    cb->draw(4);
}

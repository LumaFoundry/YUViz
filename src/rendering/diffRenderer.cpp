#include "diffRenderer.h"
#include <QFile>

DiffRenderer::DiffRenderer(QObject* parent, std::shared_ptr<FrameMeta> metaPtr) :
    QObject(parent),
    m_metaPtr(metaPtr) {
}

DiffRenderer::~DiffRenderer() = default;

void DiffRenderer::initialize(QRhi* rhi, QRhiRenderPassDescriptor* rp) {
    m_rhi = rhi;

    if (m_rhi) {
        qDebug() << m_rhi->backendName() << m_rhi->driverInfo();
    } else {
        qWarning() << "Failed to initialize RHI";
        emit rendererError();
        return;
    }

    // Create YUV textures
    m_yTex1.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_metaPtr->yWidth(), m_metaPtr->yHeight())));
    m_yTex2.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_metaPtr->yWidth(), m_metaPtr->yHeight())));
    m_yTex1->create();
    m_yTex2->create();

    // Diff configuration buffer
    m_diffConfig.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(int) * 4));
    m_diffConfig->create();

    // Set default configuration
    setDiffConfig(0, 4.0f, 0); // Default grey mode, 4x multiplier, direct subtraction

    m_resizeParams.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(float) * 4));
    m_resizeParams->create();

    // Load shaders
    Q_INIT_RESOURCE(videoplayer_shaders);
    QByteArray vsQsb = loadShaderSource(":/shaders/vertex.vert.qsb");
    QByteArray fsQsb = loadShaderSource(":/shaders/fragment-diff.frag.qsb");

    if (vsQsb.isEmpty() || fsQsb.isEmpty()) {
        qWarning() << "Failed to open shader file";
        emit rendererError();
        return;
    }

    QShader vs = QShader::fromSerialized(vsQsb);
    QShader fs = QShader::fromSerialized(fsQsb);

    // Graphics pipeline
    m_pip.reset(m_rhi->newGraphicsPipeline());
    m_pip->setShaderStages({{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
    QRhiVertexInputLayout vil;
    vil.setBindings({{sizeof(float) * 4}});
    vil.setAttributes(
        {{0, 0, QRhiVertexInputAttribute::Float2, 0}, {0, 1, QRhiVertexInputAttribute::Float2, sizeof(float) * 2}});
    m_pip->setVertexInputLayout(vil);
    m_pip->setCullMode(QRhiGraphicsPipeline::None);
    m_pip->setTargetBlends({QRhiGraphicsPipeline::TargetBlend()});
    m_pip->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_pip->setSampleCount(1); // single-sample target
    m_pip->setDepthTest(false);
    m_pip->setDepthWrite(false);
    m_pip->setRenderPassDescriptor(rp);
    m_sampler.reset(m_rhi->newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None, QRhiSampler::Repeat, QRhiSampler::Repeat));
    m_sampler->create();

    m_resourceBindings.reset(m_rhi->newShaderResourceBindings());
    m_resourceBindings->setBindings(
        {QRhiShaderResourceBinding::sampledTexture(
             1, QRhiShaderResourceBinding::FragmentStage, m_yTex1.get(), m_sampler.get()),
         QRhiShaderResourceBinding::sampledTexture(
             2, QRhiShaderResourceBinding::FragmentStage, m_yTex2.get(), m_sampler.get()),
         QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, m_diffConfig.get()),
         QRhiShaderResourceBinding::uniformBuffer(5, QRhiShaderResourceBinding::VertexStage, m_resizeParams.get())});
    m_resourceBindings->create();

    m_pip->setShaderResourceBindings(m_resourceBindings.get());
    m_pip->create();

    // Vertex buffer
    struct V {
        float x, y, u, v;
    };
    V verts[4] = {{-1, -1, 0, 1}, {-1, 1, 0, 0}, {1, -1, 1, 1}, {1, 1, 1, 0}};
    m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(verts)));
    m_vbuf->create();

    m_initBatch = m_rhi->nextResourceUpdateBatch();
    m_initBatch->uploadStaticBuffer(m_vbuf.get(), 0, sizeof(verts), verts);
}

QByteArray DiffRenderer::loadShaderSource(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

void DiffRenderer::setDiffConfig(int displayMode, float diffMultiplier, int diffMethod) {
    struct DiffConfig {
        int displayMode;
        float diffMultiplier;
        int diffMethod;
        int padding;
    };
    DiffConfig dc = {displayMode, diffMultiplier, diffMethod, 0};
    m_diffConfigBatch = m_rhi->nextResourceUpdateBatch();
    m_diffConfigBatch->updateDynamicBuffer(m_diffConfig.get(), 0, sizeof(dc), &dc);
}

void DiffRenderer::uploadFrame(FrameData* frame1, FrameData* frame2) {
    if (!frame1 || !frame2) {
        qDebug() << "DiffRenderer::uploadFrame called with invalid frame";
        emit rendererError();
        return;
    }

    // Verify that both frames have the same PTS before processing
    if (frame1->pts() != frame2->pts()) {
        qWarning() << "DiffRenderer: Skipping upload - frames have different PTS values";
        qWarning() << "Frame1 PTS:" << frame1->pts() << "Frame2 PTS:" << frame2->pts();
        emit rendererError();
        return;
    }

    m_currentPts1 = frame1->pts();
    m_currentPts2 = frame2->pts();

    qDebug() << "DiffRenderer: Uploading frames with PTS:" << m_currentPts1;

    m_frameBatch = m_rhi->nextResourceUpdateBatch();

    QRhiTextureUploadDescription yDesc1;
    {
        QRhiTextureSubresourceUploadDescription sd(frame1->yPtr(), m_metaPtr->yWidth() * m_metaPtr->yHeight());
        sd.setDataStride(m_metaPtr->yWidth());
        yDesc1.setEntries({{0, 0, sd}});
    }
    m_frameBatch->uploadTexture(m_yTex1.get(), yDesc1);

    QRhiTextureUploadDescription yDesc2;
    {
        QRhiTextureSubresourceUploadDescription sd(frame2->yPtr(), m_metaPtr->yWidth() * m_metaPtr->yHeight());
        sd.setDataStride(m_metaPtr->yWidth());
        yDesc2.setEntries({{0, 0, sd}});
    }
    m_frameBatch->uploadTexture(m_yTex2.get(), yDesc2);

    emit batchIsFull();
}

void DiffRenderer::renderFrame(QRhiCommandBuffer* cb, const QRect& viewport, QRhiRenderTarget* rt) {
    // qDebug() << "DiffRenderer:: renderFrame called";

    if (m_initBatch) {
        cb->resourceUpdate(m_initBatch);
        m_initBatch = nullptr;
    }
    if (m_diffConfigBatch) {
        cb->resourceUpdate(m_diffConfigBatch);
        m_diffConfigBatch = nullptr;
    }
    if (m_frameBatch) {
        cb->resourceUpdate(m_frameBatch);
        m_frameBatch = nullptr;
        emit batchIsEmpty();
    }

    // qDebug() << "DiffRenderer::init ready";

    // Preserve aspect ratio by computing a letterboxed viewport
    float windowAspect = float(viewport.width()) / viewport.height();

    if (std::abs(windowAspect - m_windowAspect) > 1e-4f) {
        m_windowAspect = windowAspect;

        float videoAspect = float(m_metaPtr->yWidth()) / m_metaPtr->yHeight();
        float scaleX, scaleY;

        if (windowAspect > videoAspect) {
            // window is wider than video: height fits, width letterboxed
            scaleX = videoAspect / windowAspect;
            scaleY = 1.0f;
        } else {
            // window is taller than video: width fits, height letterboxed
            scaleX = 1.0f;
            scaleY = windowAspect / videoAspect;
        }

        float offsetX = 0.0f;
        float offsetY = 0.0f;

        if (m_zoom != 1.0f) {
            // Apply zoom factor
            scaleX *= m_zoom;
            scaleY *= m_zoom;

            // Calculate offset to center the zoomed region
            offsetX = -(m_centerX - 0.5f) * 2.0f * scaleX;
            offsetY = (m_centerY - 0.5f) * 2.0f * scaleY;
        }

        // Struct matching the shader's std140 layout.
        struct ResizeParams {
            float scaleX, scaleY;
            float offsetX, offsetY;
        };

        ResizeParams rp{scaleX, scaleY, offsetX, offsetY};

        m_resizeParamsBatch = m_rhi->nextResourceUpdateBatch();
        m_resizeParamsBatch->updateDynamicBuffer(m_resizeParams.get(), 0, sizeof(rp), &rp);
    }

    if (m_resizeParamsBatch) {
        cb->resourceUpdate(m_resizeParamsBatch);
        m_resizeParamsBatch = nullptr;
    }
    cb->setViewport(QRhiViewport(viewport.x(), viewport.y(), viewport.width(), viewport.height()));

    // Draw
    cb->setGraphicsPipeline(m_pip.get());
    QRhiCommandBuffer::VertexInput vi(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vi);
    cb->setShaderResources();

    cb->draw(4);
}

void DiffRenderer::releaseBatch() {
    if (m_initBatch) {
        m_initBatch->release();
        m_initBatch = nullptr;
    }
    if (m_diffConfigBatch) {
        m_diffConfigBatch->release();
        m_diffConfigBatch = nullptr;
    }
    if (m_frameBatch) {
        m_frameBatch->release();
        m_frameBatch = nullptr;
    }
    if (m_resizeParamsBatch) {
        m_resizeParamsBatch->release();
        m_resizeParamsBatch = nullptr;
    }
}

void DiffRenderer::setZoomAndOffset(const float zoom, const float centerX, const float centerY) {
    m_zoom = zoom;
    m_centerX = centerX;
    m_centerY = centerY;
    m_windowAspect = 0.0f;
}

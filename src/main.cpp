#include <QGuiApplication>
#include <QQuickWindow>
#include <QFile>
#include <QDebug>
#include <rhi/qshader.h>
#include <rhi/qrhi.h>
#include "frames/frameQueue.h"
#include "frames/frameMeta.h"

// Helper to load binary or text data
static QByteArray loadShaderSource(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open shader file" << path;
        return {};
    }
    return f.readAll();
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QQuickWindow window;
    window.setTitle("YUV420P QQuickWindow Renderer");
    window.resize(900, 600);
    window.show();

    // --- Frame generation ---
    FrameMeta meta;
    meta.setYWidth(900);
    meta.setYHeight(600);
    meta.setUVWidth(450);
    meta.setUVHeight(300);
    meta.setPixelFormat(AV_PIX_FMT_YUV420P);
    meta.setColorRange(AVColorRange::AVCOL_RANGE_MPEG);
    meta.setColorSpace(AVColorSpace::AVCOL_SPC_BT709);

    FrameQueue queue(meta);
    FrameData *data = queue.getTailFrame();
    if (!data) {
        qWarning() << "Failed to get frame data";
        return -1;
    }
    const int yWidth  = meta.yWidth();
    const int yHeight = meta.yHeight();
    const int uvWidth  = meta.uvWidth();
    const int uvHeight = meta.uvHeight();
    uint8_t *yPlane = data->yPtr();
    uint8_t *uPlane = data->uPtr();
    uint8_t *vPlane = data->vPtr();

    struct YUV { uint8_t y,u,v; };
    YUV grid[3][3] = {
        { {16,128,128}, {128,128,128}, {235,128,128} },
        { {63,102,240}, {32,240,118},  {173,42,26}  },
        { {74,202,230}, {212,16,138},  {191,196,16} }
    };
    const int tileY = yHeight / 3;
    const int tileX = yWidth  / 3;
    for (int gy = 0; gy < 3; ++gy) {
        for (int gx = 0; gx < 3; ++gx) {
            auto col = grid[gy][gx];
            // Y plane
            for (int y = gy * tileY; y < (gy + 1) * tileY; ++y)
                for (int x = gx * tileX; x < (gx + 1) * tileX; ++x)
                    yPlane[y * yWidth + x] = col.y;
            // U/V planes (half resolution)
            for (int y = (gy * tileY) / 2; y < ((gy + 1) * tileY) / 2; ++y)
                for (int x = (gx * tileX) / 2; x < ((gx + 1) * tileX) / 2; ++x) {
                    uPlane[y * uvWidth + x] = col.u;
                    vPlane[y * uvWidth + x] = col.v;
                }
        }
    }

    // --- QRhi setup ---
    // Create RHI instance for Metal on macOS
    QRhiMetalInitParams initParams;
    initParams.window = &window;
    QRhi *rhi = QRhi::create(QRhi::Implementation::Metal, &initParams);
    if (!rhi) {
        qWarning() << "Failed to create QRhi";
        return -1;
    }

    // SwapChain tied to QQuickWindow
    QRhiSwapChain *swapChain = rhi->newSwapChain(QRhiSwapChain::Flags(), &window);
    swapChain->createOrResize();
    // Rebuild on resize
    QObject::connect(&window, &QQuickWindow::widthChanged, [&](int){
        swapChain->createOrResize();
    });
    QObject::connect(&window, &QQuickWindow::heightChanged, [&](int){
        swapChain->createOrResize();
    });

    // Create Y/U/V textures
    QRhiTexture *yTex = rhi->newTexture(QRhiTexture::R8, QSize(yWidth, yHeight));
    QRhiTexture *uTex = rhi->newTexture(QRhiTexture::R8, QSize(uvWidth, uvHeight));
    QRhiTexture *vTex = rhi->newTexture(QRhiTexture::R8, QSize(uvWidth, uvHeight));
    yTex->setFlags(QRhiTexture::UsedAsTransferSource);
    uTex->setFlags(QRhiTexture::UsedAsTransferSource);
    vTex->setFlags(QRhiTexture::UsedAsTransferSource);

    if (!yTex->create() || !uTex->create() || !vTex->create()) {
        qWarning() << "Failed to create YUV textures";
        return -1;
    }

    // Initial upload of planes
    {
        QRhiResourceUpdateBatch *batch0 = rhi->nextResourceUpdateBatch();
        // Prepare upload description with explicit row pitch
        QRhiTextureUploadDescription descY(yTex, 0, 0, yPlane, yWidth);
        batch0->uploadTexture(yTex, descY);
        QRhiTextureUploadDescription descU(uTex, 0, 0, uPlane, uvWidth);
        batch0->uploadTexture(uTex, descU);
        QRhiTextureUploadDescription descV(vTex, 0, 0, vPlane, uvWidth);
        batch0->uploadTexture(vTex, descV);
        rhi->resourceUpdate(batch0);
    }

    // --- Load precompiled shaders (.qsb) via QShader ---
    QByteArray vsQsb = loadShaderSource("shaders/vertex.qsb");
    QByteArray fsQsb = loadShaderSource("shaders/fragment.qsb");
    QShader vs = QShader::fromSerialized(vsQsb);
    QShader fs = QShader::fromSerialized(fsQsb);
    QRhiShaderStage shaderStages[2] = {
        { QRhiShaderStage::Vertex,   vs },
        { QRhiShaderStage::Fragment, fs }
    };

    // --- Graphics pipeline ---
    QRhiGraphicsPipeline *pip = rhi->newGraphicsPipeline();
    pip->setShaderStages(shaderStages, 2);

    QRhiVertexInputLayout vil;
    vil.setBindings({ { sizeof(float) * 4 } });
    QRhiVertexInputAttribute via[2] = {
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 1, 0, QRhiVertexInputAttribute::Float2, sizeof(float) * 2 }
    };
    vil.setAttributes(via, 2);
    pip->setVertexInputLayout(vil);

    QRhiGraphicsPipelineState ps;
    ps.rasterizer.cullMode = QRhiGraphicsPipelineState::None;
    ps.colorTargetCount    = 1;
    ps.colorTargets[0].format = swapChain->preferredFormat();
    pip->setGraphicsPipelineState(ps);
    pip->setRenderPassDescriptor(swapChain->renderPassDescriptor());
    pip->create();

    // --- Fullscreen quad ---
    struct V { float x,y,u,v; };
    V verts[4] = {
        {-1.0f, -1.0f, 0.0f, 1.0f},
        {-1.0f,  1.0f, 0.0f, 0.0f},
        { 1.0f, -1.0f, 1.0f, 1.0f},
        { 1.0f,  1.0f, 1.0f, 0.0f}
    };
    QRhiBuffer *vbuf = rhi->newBuffer(QRhiBuffer::Immutable,
                                      QRhiBuffer::VertexBuffer,
                                      sizeof(verts));
    vbuf->create();
    {
        QRhiResourceUpdateBatch *initBatch = rhi->nextResourceUpdateBatch();
        initBatch->uploadBuffer(vbuf, 0, verts, sizeof(verts));
        rhi->resourceUpdate(initBatch);
    }

    // --- Sampler & resource bindings ---
    QRhiSampler *sam = rhi->newSampler();
    sam->setMinMagFilters(QRhiSampler::Linear, QRhiSampler::Linear);
    sam->create();

    QRhiResourceBindings *rb = rhi->newResourceBindings();
    rb->setBindings({
        QRhiResourceBindings::SamplerBinding(0, sam),
        QRhiResourceBindings::TextureBinding(sam, yTex, 1),
        QRhiResourceBindings::TextureBinding(sam, uTex, 2),
        QRhiResourceBindings::TextureBinding(sam, vTex, 3)
    });
    rb->create();

    // --- Render loop via renderRequested ---
    QObject::connect(&window, &QQuickWindow::renderRequested,
                     [&](QQuickWindow *w){
        swapChain->prepare();
        QRhiCommandBuffer *cb = swapChain->currentFrameCommandBuffer();
        cb->beginPass(swapChain->currentFrameRenderTarget(),
                      QColor(0,0,0,255), { 1.0f, 0 });
        cb->setGraphicsPipeline(pip);
        cb->setViewport(QRect(0,0, w->width(), w->height()));
        cb->setVertexInput(0, vbuf, 0);
        cb->setResourceBindings(rb);
        cb->draw(4);
        cb->endPass();
        rhi->finishFrame(swapChain);
    }, Qt::DirectConnection);

    // Trigger the first frame render
    window.update();

    return app.exec();
}
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
    window.setTitle("videoplayer");
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
    QRhiMetalInitParams params;
    QRhi *rhi = QRhi::create(QRhi::Metal, &params);
    if (!rhi) {
        qWarning() << "Failed to create QRhi";
        return -1;
    }

    // SwapChain tied to QQuickWindow
    QRhiSwapChain *swapChain = rhi->newSwapChain();
    swapChain->setWindow(&window);
    QRhiRenderPassDescriptor *rpDesc = swapChain->newCompatibleRenderPassDescriptor();
    swapChain->setRenderPassDescriptor(rpDesc);
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

    if (!yTex->create() || !uTex->create() || !vTex->create()) {
        qWarning() << "Failed to create YUV textures";
        return -1;
    }

    // Initial upload of planes
    {
        QRhiResourceUpdateBatch *batch0 = rhi->nextResourceUpdateBatch();
        // Prepare and upload Y plane using QRhiTextureSubresourceUploadDescription
        QRhiTextureUploadDescription descY;
        {
            QRhiTextureSubresourceUploadDescription subDescY(yPlane, yWidth * yHeight);
            subDescY.setDataStride(yWidth);
            QRhiTextureUploadEntry entryY(0, 0, subDescY);
            descY.setEntries({ entryY });
        }
        batch0->uploadTexture(yTex, descY);

        // Prepare and upload U plane
        QRhiTextureUploadDescription descU;
        {
            QRhiTextureSubresourceUploadDescription subDescU(uPlane, uvWidth * uvHeight);
            subDescU.setDataStride(uvWidth);
            QRhiTextureUploadEntry entryU(0, 0, subDescU);
            descU.setEntries({ entryU });
        }
        batch0->uploadTexture(uTex, descU);

        // Prepare and upload V plane
        QRhiTextureUploadDescription descV;
        {
            QRhiTextureSubresourceUploadDescription subDescV(vPlane, uvWidth * uvHeight);
            subDescV.setDataStride(uvWidth);
            QRhiTextureUploadEntry entryV(0, 0, subDescV);
            descV.setEntries({ entryV });
        }
        batch0->uploadTexture(vTex, descV);
        // Submit resource updates via the swap chain's command buffer
        // Submit resource updates within a frame
        rhi->beginFrame(swapChain);
        QRhiCommandBuffer *cmdBuf = swapChain->currentFrameCommandBuffer();
        cmdBuf->resourceUpdate(batch0);
        rhi->endFrame(swapChain);
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
    pip->setShaderStages({ shaderStages[0], shaderStages[1] });

    QRhiVertexInputLayout vil;
    vil.setBindings({ { sizeof(float) * 4 } });
    QRhiVertexInputAttribute via[2] = {
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 1, 0, QRhiVertexInputAttribute::Float2, sizeof(float) * 2 }
    };
    vil.setAttributes(via, via + 2);
    pip->setVertexInputLayout(vil);

    // Configure pipeline state via QRhiGraphicsPipeline setters (Qt 6.9)
    pip->setCullMode(QRhiGraphicsPipeline::None);
    // One render target, no blending
    pip->setTargetBlends({ QRhiGraphicsPipeline::TargetBlend() });
    // Primitive topology
    pip->setTopology(QRhiGraphicsPipeline::Triangles);
    // Match the swap chain's sample count
    pip->setSampleCount(swapChain->sampleCount());
    // No depth or stencil
    pip->setDepthTest(false);
    pip->setDepthWrite(false);
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
        initBatch->uploadStaticBuffer(vbuf, 0, sizeof(verts), verts);
        // Submit static buffer upload via command buffer
        rhi->beginFrame(swapChain);
        QRhiCommandBuffer *cmdBuf = swapChain->currentFrameCommandBuffer();
        cmdBuf->resourceUpdate(initBatch);
        rhi->endFrame(swapChain);
    }

    // --- Sampler & resource bindings ---
    // Create sampler with linear filtering and repeat addressing
    QRhiSampler *sam = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                       QRhiSampler::None,
                                       QRhiSampler::Repeat, QRhiSampler::Repeat);
    sam->create();

    // Create shader resource bindings for the Y, U, V textures
    QRhiShaderResourceBindings *rb = rhi->newShaderResourceBindings();
    rb->setBindings({
        // Bind each plane texture + sampler to fragment shader
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, yTex, sam),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, uTex, sam),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, vTex, sam)
    });
    rb->create();
    // Attach to pipeline
    pip->setShaderResourceBindings(rb);

    // --- Render loop via beforeRendering ---
    QObject::connect(&window, &QQuickWindow::beforeRendering,
                     &window,
                     [&](){
        // Begin a new frame instead of deprecated prepare()
        rhi->beginFrame(swapChain);
        QRhiCommandBuffer *cb = swapChain->currentFrameCommandBuffer();
        cb->beginPass(swapChain->currentFrameRenderTarget(),
                      QColor(0,0,0,255), { 1.0f, 0 });
        cb->setGraphicsPipeline(pip);
        // Set viewport using QRhiViewport instead of QRect
        cb->setViewport(QRhiViewport(0, 0, window.width(), window.height()));
        // Bind vertex buffer at binding point 0 with offset 0
        QRhiCommandBuffer::VertexInput viBinding(vbuf, 0);
        cb->setVertexInput(0, 1, &viBinding);
        cb->setShaderResources();  // bind the resources to the current pipeline
        cb->draw(4);
        cb->endPass();
        // End the frame instead of deprecated finishFrame()
        rhi->endFrame(swapChain);
    }, Qt::DirectConnection);

    // Trigger the first frame render
    window.update();

    return app.exec();
}
#include <QGuiApplication>
#include <QWindow>
#include <QSurface>
#include <QTimer>
#include <QFile>
#include <QDebug>
#include <vector>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <rhi/qshaderdescription.h>
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

    // --- Window setup ---
    QWindow window;
#if defined(Q_OS_MACOS)
    window.setSurfaceType(QSurface::MetalSurface);
#elif defined(Q_OS_WIN)
    window.setSurfaceType(QSurface::OpenGLSurface);
#else
    window.setSurfaceType(QSurface::VulkanSurface);
#endif
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
    const int yWidth   = meta.yWidth();
    const int yHeight  = meta.yHeight();
    const int uvWidth  = meta.uvWidth();
    const int uvHeight = meta.uvHeight();
    uint8_t *yPlane = data->yPtr();
    uint8_t *uPlane = data->uPtr();
    uint8_t *vPlane = data->vPtr();

    // Fill with typical test colors in YUV
    struct YUV { uint8_t y,u,v; };
    YUV grid[3][3] = {
    { {  76,  85, 255}, 
        { 150,  44,  21}, 
        {  29, 255, 107} },

    { {165,  42, 179},  
        { 61, 165, 175}, 
        {170, 166,  16} }, 

    { {235,128,128},   
        {128,128,128},  
        { 16,128,128} } 
    };
    const int tileY = yHeight / 3;
    const int tileX = yWidth  / 3;
    for (int gy = 0; gy < 3; ++gy) {
        for (int gx = 0; gx < 3; ++gx) {
            auto col = grid[gy][gx];
            for (int y = gy * tileY; y < (gy + 1) * tileY; ++y)
                for (int x = gx * tileX; x < (gx + 1) * tileX; ++x)
                    yPlane[y * yWidth + x] = col.y;
            for (int y = (gy * tileY) / 2; y < ((gy + 1) * tileY) / 2; ++y)
                for (int x = (gx * tileX) / 2; x < ((gx + 1) * tileX) / 2; ++x) {
                    uPlane[y * uvWidth + x] = col.u;
                    vPlane[y * uvWidth + x] = col.v;
                }
        }
    }

    // --- QRhi setup ---
    QRhi *rhi = nullptr;

    #if defined(Q_OS_MACOS)
        // --- Pure RHI Metal setup ---
        QRhiMetalInitParams initParams;
        rhi = QRhi::create(QRhi::Implementation::Metal, &initParams);
        if (!rhi) {
            qWarning() << "Failed to create Metal QRhi backend";
            return -1;
        }
    #elif defined(Q_OS_WIN)
        // D3D11 on Windows
        QRhiD3D11InitParams d3dParams;
        rhi = QRhi::create(QRhi::D3D11, &d3dParams);
        if (!rhi) {
            qWarning() << "Failed to create D3D11 backend, trying OpenGL...";
            QRhiGles2InitParams glParams;
            rhi = QRhi::create(QRhi::OpenGLES2, &glParams);
        }
    #else
        // Vulkan on Linux
        QRhiVulkanInitParams vulkanParams;
        rhi = QRhi::create(QRhi::Vulkan, &vulkanParams);
        if (!rhi) {
            qWarning() << "Failed to create Vulkan backend, trying OpenGL...";
            QRhiGles2InitParams glParams;
            rhi = QRhi::create(QRhi::OpenGLES2, &glParams);
        }
    #endif

    if (!rhi) {
        qWarning() << "Failed to create any QRhi backend";
        return -1;
    }

    // Swap chain tied to QWindow
    QRhiSwapChain *swapChain = rhi->newSwapChain();
    swapChain->setWindow(&window);
    auto rpDesc = swapChain->newCompatibleRenderPassDescriptor();
    swapChain->setRenderPassDescriptor(rpDesc);
    swapChain->createOrResize();

    QObject::connect(&window, &QWindow::widthChanged, [&](int){ swapChain->createOrResize(); });
    QObject::connect(&window, &QWindow::heightChanged, [&](int){ swapChain->createOrResize(); });

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
        auto batch0 = rhi->nextResourceUpdateBatch();
        // Y plane
        QRhiTextureUploadDescription descY;
        {
            QRhiTextureSubresourceUploadDescription sd(yPlane, yWidth*yHeight);
            sd.setDataStride(yWidth);
            descY.setEntries({ {0,0,sd} });
        }
        batch0->uploadTexture(yTex, descY);
        // U plane
        QRhiTextureUploadDescription descU;
        {
            QRhiTextureSubresourceUploadDescription sd(uPlane, uvWidth*uvHeight);
            sd.setDataStride(uvWidth);
            descU.setEntries({ {0,0,sd} });
        }
        batch0->uploadTexture(uTex, descU);
        // V plane
        QRhiTextureUploadDescription descV;
        {
            QRhiTextureSubresourceUploadDescription sd(vPlane, uvWidth*uvHeight);
            sd.setDataStride(uvWidth);
            descV.setEntries({ {0,0,sd} });
        }
        batch0->uploadTexture(vTex, descV);

        // Submit in one frame
        rhi->beginFrame(swapChain);
        auto cb0 = swapChain->currentFrameCommandBuffer();
        cb0->resourceUpdate(batch0);
        rhi->endFrame(swapChain);
    }

    // --- Load shaders (.qsb) ---
    QByteArray vsQsb = loadShaderSource("../src/shaders/vertex.qsb");
    QByteArray fsQsb = loadShaderSource("../src/shaders/fragment.qsb");
    if (vsQsb.isEmpty() || fsQsb.isEmpty()) {
        qFatal("Missing .qsb shader files");
    }
    QShader vs = QShader::fromSerialized(vsQsb);
    QShader fs = QShader::fromSerialized(fsQsb);

    // --- Graphics pipeline ---
    QRhiGraphicsPipeline *pip = rhi->newGraphicsPipeline();
    pip->setShaderStages({
        { QRhiShaderStage::Vertex,   vs },
        { QRhiShaderStage::Fragment, fs }
    });
    
    // Static vertex input layout for interleaved vec2 position + vec2 texCoord
    QRhiVertexInputLayout vil;
    vil.setBindings({ { sizeof(float)*4 } });
    vil.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, sizeof(float)*2 }
    });
    pip->setVertexInputLayout(vil);

    // Pipeline state
    pip->setCullMode(QRhiGraphicsPipeline::None);
    pip->setTargetBlends({ QRhiGraphicsPipeline::TargetBlend() });
    pip->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    pip->setSampleCount(swapChain->sampleCount());
    pip->setDepthTest(false);
    pip->setDepthWrite(false);
    pip->setRenderPassDescriptor(swapChain->renderPassDescriptor());
    
    // Sampler + resource bindings
    QRhiSampler *sam = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                       QRhiSampler::None,
                                       QRhiSampler::Repeat, QRhiSampler::Repeat);
    sam->create();
    QRhiShaderResourceBindings *rb = rhi->newShaderResourceBindings();
    rb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, yTex, sam),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, uTex, sam),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, vTex, sam)
    });
    rb->create();
    pip->setShaderResourceBindings(rb);
    pip->create();

    // --- Fullscreen quad buffer ---
    struct V { float x,y,u,v; };
    V verts[4] = {
        {-1,-1, 0,1}, {-1,1, 0,0}, {1,-1, 1,1}, {1,1,1,0}
    };
    QRhiBuffer *vbuf = rhi->newBuffer(QRhiBuffer::Immutable,
                                      QRhiBuffer::VertexBuffer,
                                      sizeof(verts));
    vbuf->create();
    {
        auto initBatch = rhi->nextResourceUpdateBatch();
        initBatch->uploadStaticBuffer(vbuf, 0, sizeof(verts), verts);
        rhi->beginFrame(swapChain);
        auto cb1 = swapChain->currentFrameCommandBuffer();
        cb1->resourceUpdate(initBatch);
        rhi->endFrame(swapChain);
    }

    // --- Render loop via QTimer (~60Hz) ---
    QTimer *timer = new QTimer(&window);
    timer->setInterval(16);
    QObject::connect(timer, &QTimer::timeout, &window, [&](){
        rhi->beginFrame(swapChain);
        auto cb = swapChain->currentFrameCommandBuffer();
        cb->beginPass(swapChain->currentFrameRenderTarget(),
                      QColor(0,0,0,255), {1.0f,0});
        cb->setGraphicsPipeline(pip);
        cb->setViewport(QRhiViewport(0,0, window.width(), window.height()));
        QRhiCommandBuffer::VertexInput vi(vbuf, 0);
        cb->setVertexInput(0,1,&vi);
        cb->setShaderResources();
        cb->draw(4);
        cb->endPass();
        rhi->endFrame(swapChain);
    });
    timer->start();

    return app.exec();
}
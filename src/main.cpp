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
    window.setSurfaceType(QSurface::Direct3DSurface);
#elif defined(Q_OS_LINUX)
    QVulkanInstance vulkanInst;
    vulkanInst.setLayers({ "VK_LAYER_KHRONOS_validation" });
    vulkanInst.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
    window.setSurfaceType(QSurface::VulkanSurface);
    if (!vulkanInst.create()) {
        qWarning("Failed to create Vulkan instance, switching to OpenGL");
        window.setSurfaceType(QSurface::OpenGLSurface);
    }
    window.setVulkanInstance(&vulkanInst);
#else
    window.setSurfaceType(QSurface::OpenGLSurface);
#endif
    window.setTitle("videoplayer");
    window.resize(300, 1200);
    window.show();

    // --- Frame generation ---
    FrameMeta meta;
    meta.setYWidth(300);
    meta.setYHeight(1200);
    meta.setUVWidth(150);
    meta.setUVHeight(600);
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

    // Extended test color matrix: 2 rows × 3 columns, each pattern contains six basic colors (black, white, gray, red, green, blue)
    // First row: MPEG range (16-235)
    // Second row: JPEG range (0-255)
    // Columns: Three main color space standards, each standard uses corresponding YUV values to ensure purest RGB
    struct YUV { uint8_t y,u,v; };
    
    // Six basic colors' RGB values (for calculating YUV)
    struct RGB { float r, g, b; };
    RGB pureColors[6] = {
        {0.0f, 0.0f, 0.0f},  // Black
        {1.0f, 1.0f, 1.0f},  // White
        {0.5f, 0.5f, 0.5f},  // Gray
        {1.0f, 0.0f, 0.0f},  // Red
        {0.0f, 1.0f, 0.0f},  // Green
        {0.0f, 0.0f, 1.0f}   // Blue
    };
    
    // RGB to YUV conversion function (BT.709)
    auto rgbToYuv709 = [](const RGB& rgb) -> YUV {
        float y = 0.2126f * rgb.r + 0.7152f * rgb.g + 0.0722f * rgb.b;
        float u = -0.1146f * rgb.r - 0.3854f * rgb.g + 0.5000f * rgb.b;
        float v = 0.5000f * rgb.r - 0.4542f * rgb.g - 0.0458f * rgb.b;
        return {(uint8_t)(y * 255), (uint8_t)((u + 0.5f) * 255), (uint8_t)((v + 0.5f) * 255)};
    };
    
    // RGB to YUV conversion function (BT.601)
    auto rgbToYuv601 = [](const RGB& rgb) -> YUV {
        float y = 0.2990f * rgb.r + 0.5870f * rgb.g + 0.1140f * rgb.b;
        float u = -0.1687f * rgb.r - 0.3313f * rgb.g + 0.5000f * rgb.b;
        float v = 0.5000f * rgb.r - 0.4187f * rgb.g - 0.0813f * rgb.b;
        return {(uint8_t)(y * 255), (uint8_t)((u + 0.5f) * 255), (uint8_t)((v + 0.5f) * 255)};
    };
    
    // RGB to YUV conversion function (BT.2020)
    auto rgbToYuv2020 = [](const RGB& rgb) -> YUV {
        float y = 0.2627f * rgb.r + 0.6780f * rgb.g + 0.0593f * rgb.b;
        float u = -0.1396f * rgb.r - 0.3604f * rgb.g + 0.5000f * rgb.b;
        float v = 0.5000f * rgb.r - 0.4598f * rgb.g - 0.0402f * rgb.b;
        return {(uint8_t)(y * 255), (uint8_t)((u + 0.5f) * 255), (uint8_t)((v + 0.5f) * 255)};
    };
    
    // Calculate correct YUV values for each standard
    YUV bt709Colors[6], bt601Colors[6], bt2020Colors[6];
    for (int i = 0; i < 6; ++i) {
        bt709Colors[i] = rgbToYuv709(pureColors[i]);
        bt601Colors[i] = rgbToYuv601(pureColors[i]);
        bt2020Colors[i] = rgbToYuv2020(pureColors[i]);
    }
    
    // Adjust to MPEG range (16-235 for Y, 16-240 for U/V)
    auto adjustToMpegRange = [](const YUV& yuv) -> YUV {
        uint8_t y = (uint8_t)(16 + (yuv.y - 0) * (235 - 16) / 255);
        uint8_t u = (uint8_t)(16 + (yuv.u - 0) * (240 - 16) / 255);
        uint8_t v = (uint8_t)(16 + (yuv.v - 0) * (240 - 16) / 255);
        return {y, u, v};
    };
    
    // Adjust to JPEG range (0-255)
    auto adjustToJpegRange = [](const YUV& yuv) -> YUV {
        return yuv; // Already in 0-255 range
    };
    
    // 2 rows × 3 columns test matrix
    YUV testMatrix[2][3][6] = {
        // First row: MPEG range (16-235)
        {
            // Column 0: BT.709 (AVCOL_SPC_BT709 = 1)
            {adjustToMpegRange(bt709Colors[0]), adjustToMpegRange(bt709Colors[1]), adjustToMpegRange(bt709Colors[2]), 
             adjustToMpegRange(bt709Colors[3]), adjustToMpegRange(bt709Colors[4]), adjustToMpegRange(bt709Colors[5])},
            // Column 1: BT.601 (AVCOL_SPC_BT470BG = 5, AVCOL_SPC_SMPTE170M = 6)
            {adjustToMpegRange(bt601Colors[0]), adjustToMpegRange(bt601Colors[1]), adjustToMpegRange(bt601Colors[2]), 
             adjustToMpegRange(bt601Colors[3]), adjustToMpegRange(bt601Colors[4]), adjustToMpegRange(bt601Colors[5])},
            // Column 2: BT.2020 (AVCOL_SPC_BT2020_NCL = 9, AVCOL_SPC_BT2020_CL = 10)
            {adjustToMpegRange(bt2020Colors[0]), adjustToMpegRange(bt2020Colors[1]), adjustToMpegRange(bt2020Colors[2]), 
             adjustToMpegRange(bt2020Colors[3]), adjustToMpegRange(bt2020Colors[4]), adjustToMpegRange(bt2020Colors[5])}
        },
        // Second row: JPEG range (0-255)
        {
            // Column 0: BT.709 (AVCOL_SPC_BT709 = 1)
            {adjustToJpegRange(bt709Colors[0]), adjustToJpegRange(bt709Colors[1]), adjustToJpegRange(bt709Colors[2]), 
             adjustToJpegRange(bt709Colors[3]), adjustToJpegRange(bt709Colors[4]), adjustToJpegRange(bt709Colors[5])},
            // Column 1: BT.601 (AVCOL_SPC_BT470BG = 5, AVCOL_SPC_SMPTE170M = 6)
            {adjustToJpegRange(bt601Colors[0]), adjustToJpegRange(bt601Colors[1]), adjustToJpegRange(bt601Colors[2]), 
             adjustToJpegRange(bt601Colors[3]), adjustToJpegRange(bt601Colors[4]), adjustToJpegRange(bt601Colors[5])},
            // Column 2: BT.2020 (AVCOL_SPC_BT2020_NCL = 9, AVCOL_SPC_BT2020_CL = 10)
            {adjustToJpegRange(bt2020Colors[0]), adjustToJpegRange(bt2020Colors[1]), adjustToJpegRange(bt2020Colors[2]), 
             adjustToJpegRange(bt2020Colors[3]), adjustToJpegRange(bt2020Colors[4]), adjustToJpegRange(bt2020Colors[5])}
        }
    };
    
    // Calculate size of each color block
    const int tileY = yHeight / 2;  // 2 rows
    const int tileX = yWidth / 3;   // 3 columns
    const int colorTileY = tileY / 6;  // Height of each color block
    const int colorTileX = tileX;      // Width of each color block
    
    // Fill YUV planes
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col) {
            for (int colorIdx = 0; colorIdx < 6; ++colorIdx) {
                auto yuvColor = testMatrix[row][col][colorIdx];
                
                // Calculate position of current color block
                int startY = row * tileY + colorIdx * colorTileY;
                int endY = startY + colorTileY;
                int startX = col * tileX;
                int endX = startX + colorTileX;
                
                // Fill Y plane
                for (int y = startY; y < endY; ++y) {
                    for (int x = startX; x < endX; ++x) {
                        yPlane[y * yWidth + x] = yuvColor.y;
                    }
                }
                
                // Fill U/V planes (note: UV plane size is half of Y plane)
                int uvStartY = startY / 2;
                int uvEndY = endY / 2;
                int uvStartX = startX / 2;
                int uvEndX = endX / 2;
                
                for (int y = uvStartY; y < uvEndY; ++y) {
                    for (int x = uvStartX; x < uvEndX; ++x) {
                        uPlane[y * uvWidth + x] = yuvColor.u;
                        vPlane[y * uvWidth + x] = yuvColor.v;
                    }
                }
            }
        }
    }

    // --- QRhi setup ---
    QRhi *rhi = nullptr;

#if defined(Q_OS_MACOS)
    QRhiMetalInitParams metalParams;
    rhi = QRhi::create(QRhi::Metal, &metalParams);
#elif defined(Q_OS_WIN)
    QRhiD3D11InitParams d3dParams;
    rhi = QRhi::create(QRhi::D3D11, &d3dParams);
#elif defined(Q_OS_LINUX)
    QRhiVulkanInitParams vulkanParams;
    vulkanParams.inst = window.vulkanInstance();
    vulkanParams.window = &window;
    rhi = QRhi::create(QRhi::Vulkan, &vulkanParams);
#endif

    if (!rhi) {
        qWarning() << "Warning: Failed to create QRhi instance, attempting to use OpenGL as fallback";
        QRhiGles2InitParams glParams;
        glParams.window = &window;
        rhi = QRhi::create(QRhi::OpenGLES2, &glParams);
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
    
    // Create color space and color range uniform buffer
    struct ColorSpaceParams {
        int colorSpace;  // AVColorSpace: 1=BT.709, 5=BT.470BG, 6=SMPTE170M, 9=BT.2020_NCL, 10=BT.2020_CL
        int colorRange;  // AVColorRange: 1=MPEG (16-235), 2=JPEG (0-255)
        int padding[2];  // Alignment to 16-byte boundary
    };
    
    QRhiBuffer *colorSpaceUBO = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ColorSpaceParams));
    colorSpaceUBO->create();
    
    // Initialize color space parameters (using video metadata)
    ColorSpaceParams colorSpaceParams;
    colorSpaceParams.colorSpace = meta.colorSpace();
    colorSpaceParams.colorRange = meta.colorRange();
    colorSpaceParams.padding[0] = 0;
    colorSpaceParams.padding[1] = 0;
    
    QRhiShaderResourceBindings *rb = rhi->newShaderResourceBindings();
    rb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, yTex, sam),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, uTex, sam),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, vTex, sam),
        QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, colorSpaceUBO)
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
        // Upload static vertex buffer
        initBatch->uploadStaticBuffer(vbuf, 0, sizeof(verts), verts);
        // Upload static color space parameters
        initBatch->updateDynamicBuffer(colorSpaceUBO, 0, sizeof(ColorSpaceParams), &colorSpaceParams);
        rhi->beginFrame(swapChain);
        auto cb1 = swapChain->currentFrameCommandBuffer();
        cb1->resourceUpdate(initBatch);
        rhi->endFrame(swapChain);
    }

    // --- Render loop via QTimer (~60Hz) ---
    QTimer *timer = new QTimer(&window);
    timer->setInterval(16);
    QObject::connect(timer, &QTimer::timeout, &window, [&](){
        // Letterbox: preserve video aspect ratio with black bars
        rhi->beginFrame(swapChain);
        auto cb = swapChain->currentFrameCommandBuffer();
        cb->beginPass(swapChain->currentFrameRenderTarget(),
                      QColor(0,0,0,255), {1.0f,0});
        cb->setGraphicsPipeline(pip);

        // Get logical window size
        const int winW = window.width();
        const int winH = window.height();

        // Get the device pixel ratio to convert logical to physical pixels
        const qreal dpr = window.devicePixelRatio();
        const int physicalW = winW * dpr;
        const int physicalH = winH * dpr;

        // Aspect ratios can be calculated with logical or physical sizes
        const float winAspect = float(winW) / winH;
        const float vidAspect = float(yWidth) / yHeight;

        int vpX = 0, vpY = 0, vpW = physicalW, vpH = physicalH;
        if (winAspect > vidAspect) {
            // Window is wider -> pillarbox (vertical bars)
            vpH = physicalH;
            vpW = int(vidAspect * vpH + 0.5f);
            vpX = (physicalW - vpW) / 2;
        } else if (vidAspect > winAspect) {
            // Window is taller -> letterbox (horizontal bars)
            vpW = physicalW;
            vpH = int(vpW / vidAspect + 0.5f);
            vpY = (physicalH - vpH) / 2;
        }
        // Set the viewport using physical pixel dimensions
        cb->setViewport(QRhiViewport(vpX, vpY, vpW, vpH));

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
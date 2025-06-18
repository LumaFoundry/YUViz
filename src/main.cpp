#include <QGuiApplication>
#include <QWindow>
#include <QSurface>
#include <QTimer>
#include <QFile>
#include <QDebug>
#include <vector>
#include <rhi/qrhi.h>
#include "frames/frameQueue.h"
#include "frames/frameMeta.h"
#include "rendering/videoRenderer.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

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
    std::shared_ptr<FrameMeta> metaPtr = std::make_shared<FrameMeta>(meta);
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

    // Instantiate and initialize VideoRenderer
    VideoRenderer *renderer = new VideoRenderer(&window, metaPtr);
#if defined(Q_OS_MACOS)
    renderer->initialize(QRhi::Metal);
#elif defined(Q_OS_WIN)
    renderer->initialize(QRhi::D3D11);
#elif defined(Q_OS_LINUX)
    renderer->initialize(QRhi::Vulkan);
#else
    renderer->initialize(QRhi::OpenGLES2);
#endif
    // Upload the first frame
    renderer->uploadFrame(data);

    QTimer *t = new QTimer(&window);
    t->setInterval(0);
    QObject::connect(t, &QTimer::timeout, [&](){
        renderer->renderFrame();
        t->stop();
    });
    t->start();

    return app.exec();
}
#include <QGuiApplication>
#include <QWindow>
#include <QSurface>
#include <QTimer>
#include <QFile>
#include <QDebug>
#include <QCommandLineParser>
#include <vector>
#include <rhi/qrhi.h>
#include "frames/frameQueue.h"
#include "frames/frameMeta.h"
#include "rendering/videoRenderer.h"
#include "ui/videoWindow.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // Code for argument parsing and selection of graphics API taken
    // from qtbase/examples/gui/rhiwindow/
    QRhi::Implementation graphicsApi;

    // Use platform-specific defaults when no command-line arguments given.
#if defined(Q_OS_WIN)
    graphicsApi = QRhi::D3D11;
#elif QT_CONFIG(metal)
    graphicsApi = QRhi::Metal;
#elif QT_CONFIG(vulkan) && defined(USE_VULKAN)
    graphicsApi = QRhi::Vulkan;
#else
    graphicsApi = QRhi::OpenGLES2;
#endif

    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    QCommandLineOption nullOption({ "n", "null" }, QLatin1String("Null"));
    cmdLineParser.addOption(nullOption);
    QCommandLineOption glOption({ "g", "opengl" }, QLatin1String("OpenGL"));
    cmdLineParser.addOption(glOption);
    QCommandLineOption vkOption({ "v", "vulkan" }, QLatin1String("Vulkan"));
    cmdLineParser.addOption(vkOption);
    QCommandLineOption d3d11Option({ "d", "d3d11" }, QLatin1String("Direct3D 11"));
    cmdLineParser.addOption(d3d11Option);
    QCommandLineOption d3d12Option({ "D", "d3d12" }, QLatin1String("Direct3D 12"));
    cmdLineParser.addOption(d3d12Option);
    QCommandLineOption mtlOption({ "m", "metal" }, QLatin1String("Metal"));
    cmdLineParser.addOption(mtlOption);

    cmdLineParser.process(app);
    if (cmdLineParser.isSet(nullOption))
        graphicsApi = QRhi::Null;
    if (cmdLineParser.isSet(glOption))
        graphicsApi = QRhi::OpenGLES2;
    if (cmdLineParser.isSet(vkOption))
        graphicsApi = QRhi::Vulkan;
    if (cmdLineParser.isSet(d3d11Option))
        graphicsApi = QRhi::D3D11;
    if (cmdLineParser.isSet(d3d12Option))
        graphicsApi = QRhi::D3D12;
    if (cmdLineParser.isSet(mtlOption))
        graphicsApi = QRhi::Metal;

#if QT_CONFIG(vulkan) && defined(USE_VULKAN)
    QVulkanInstance vulkanInst;
    if (graphicsApi == QRhi::Vulkan) {
        // possibly remove once bugs are cleared out
        vulkanInst.setLayers({ "VK_LAYER_KHRONOS_validation" });
        vulkanInst.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
        if (!vulkanInst.create()) {
            qWarning() << "Failed to create Vulkan instance";
            return -1;
        }
    }
#endif

    // use VideoWindow
    VideoWindow videoWin(nullptr, graphicsApi);
    QWindow* window = videoWin.window();

#if QT_CONFIG(vulkan) && defined(USE_VULKAN)
    if (graphicsApi == QRhi::Vulkan) {
        window->setVulkanInstance(&vulkanInst);
    }
#endif

    window->setTitle("videoplayer");
    window->resize(900, 600);
    window->show();

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
    VideoRenderer *renderer = new VideoRenderer(window, metaPtr);

    renderer->initialize(graphicsApi);

    // Upload the first frame
    renderer->uploadFrame(data);

    QTimer *t = new QTimer(window);
    t->setInterval(0);
    QObject::connect(t, &QTimer::timeout, [&](){
        renderer->renderFrame();
        t->stop();
    });
    t->start();

    return app.exec();
}
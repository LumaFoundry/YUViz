#pragma once

#include <memory>
#include <QSize>
#include <QWindow>
#include "rhi/qrhi.h"
#include "frames/frameData.h"

class VideoRenderer : public QObject {
    Q_OBJECT
public:
    VideoRenderer(QWindow* window,
                  int yWidth,
                  int yHeight,
                  int uvWidth,
                  int uvHeight);
    ~VideoRenderer();

    void initialize(QRhi::Implementation impl);

public slots:
    void uploadFrame(FrameData* frame);
    void renderFrame();

signals:
    void frameUpLoaded();
    void errorOccurred();


private:
    VideoRenderer(const VideoRenderer&) = delete;
    VideoRenderer& operator=(const VideoRenderer&) = delete;

    int m_yWidth;
    int m_yHeight;
    int m_uvWidth;
    int m_uvHeight;

    QWindow* m_window = nullptr;
    std::unique_ptr<QRhi> m_rhi;
    std::unique_ptr<QRhiSwapChain> m_swapChain;
    std::unique_ptr<QRhiTexture> m_yTex;
    std::unique_ptr<QRhiTexture> m_uTex;
    std::unique_ptr<QRhiTexture> m_vTex;
    QRhiResourceUpdateBatch* m_batch = nullptr;
    std::unique_ptr<QRhiGraphicsPipeline> m_pip;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_resourceBindings;
    std::unique_ptr<QRhiBuffer> m_vbuf;

    static QByteArray loadShaderSource(const QString &path);
};

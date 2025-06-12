#include "frameController.h"

FrameController::FrameController(QObject *parent, VideoDecoder* decoder, VideoRenderer* renderer) 
    : QObject(parent),
      m_Decoder(std::unique_ptr<VideoDecoder>(decoder)),
      m_Renderer(std::unique_ptr<VideoRenderer>(renderer)),
      m_frameQueue(m_Decoder->getMetaData())
{
    // Initialize decoder and renderer thread
    m_Decoder->moveToThread(&m_decodeThread);
    m_Renderer->moveToThread(&m_renderThread);

    // Connect signals and slots for frame decoding and rendering
    connect(this, &FrameController::requestDecode, m_Decoder.get(), &VideoDecoder::loadFrame);
    connect(this, &FrameController::requestRender, m_Renderer.get(), &VideoRenderer::renderFrame);

    connect(m_Decoder.get(), &VideoDecoder::frameLoaded, this, &FrameController::onFrameDecoded);
    connect(m_Renderer.get(), &VideoRenderer::frameRendered, this, &FrameController::onFrameRendered);
}

FrameController::~FrameController(){}

// Start the decode and render threads
void FrameController::start(){
    m_decodeThread.start();
    m_renderThread.start();

    // Start decoding first frame (logical loop head)
    emit requestDecode(m_frameQueue.getTailFrame());
}

void FrameController:: onFrameDecoded(){
    m_frameQueue.incrementTail();
    emit requestRender(m_frameQueue.getHeadFrame());
}

void FrameController:: onFrameRendered(){
    m_frameQueue.incrementHead();
    emit requestDecode(m_frameQueue.getTailFrame());
}








#include "frameController.h"

FrameController::FrameController(
        QObject *parent, 
        std::unique_ptr<VideoDecoder> decoder, 
        std::unique_ptr<VideoRenderer> renderer, 
        std::shared_ptr<PlaybackWorker> playbackWorker, 
        std::shared_ptr<VideoWindow> window, 
        int index)
    : QObject(parent),
        m_Decoder(std::move(decoder)),
        m_Renderer(std::move(renderer)),
        m_PlaybackWorker(playbackWorker),
        m_window(window),
        m_frameQueue(m_Decoder->getMetaData()),
        m_index(index)
{

    // Initialize decoder and renderer thread
    m_Decoder->moveToThread(&m_decodeThread);
    m_Renderer->moveToThread(&m_renderThread);

    // Request & Receive timer ticks and sleep for frame processing
    connect(m_PlaybackWorker.get(), &PlaybackWorker::tick, this, &FrameController::onTimerTick, Qt::QueuedConnection);

    // Request & Receive signals for decoding
    connect(this, &FrameController::requestDecode, m_Decoder.get(), &VideoDecoder::loadFrame, Qt::QueuedConnection);
    connect(m_Decoder.get(), &VideoDecoder::frameLoaded, this, &FrameController::onFrameDecoded, Qt::QueuedConnection);

    // Request & Receive signals for uploading texture to buffer
    connect(this, &FrameController::requestUpload, m_Renderer.get(), &VideoRenderer::uploadFrame, Qt::QueuedConnection);
    connect(m_Renderer.get(), &VideoRenderer::batchUploaded, this, &FrameController::onFrameUploaded, Qt::QueuedConnection);

    // Request & Receive for uploading to GPU and rendering frames
    connect(this, &FrameController::requestRender, m_Renderer.get(), &VideoRenderer::renderFrame, Qt::QueuedConnection);
    connect(m_Renderer.get(), &VideoRenderer::gpuUploaded, this, &FrameController::onFrameRendered, Qt::QueuedConnection);
    
    // Error handling for renderer
    connect(m_Renderer.get(), &VideoRenderer::errorOccurred, this, &FrameController::onRenderError, Qt::QueuedConnection);

}

FrameController::~FrameController(){
    // Ensure threads are stopped before destruction
    m_decodeThread.quit();
    m_renderThread.quit();
    m_decodeThread.wait();
    m_renderThread.wait();

    // Clear unique pointers
    m_Decoder.reset();
    m_Renderer.reset();
}

// Start the decode and render threads
void FrameController::start(){
    m_decodeThread.start();
    m_renderThread.start();

    // Temporary connection for prefill
    connect(m_Decoder.get(), &VideoDecoder::frameLoaded, this, &FrameController::onPrefillCompleted, Qt::QueuedConnection);
    
    // Initial request to decode
    emit requestDecode(m_frameQueue.getTailFrame());
}


// Slots Definitions

void FrameController::onPrefillCompleted(bool success) {
    if (!success) {
        ErrorReporter::instance().report("Prefill error occurred", LogLevel::Error);
        return;
    } else {
        if (m_prefillDecodedCount < m_frameQueue.getSize()/2) {
            emit requestDecode(m_frameQueue.getTailFrame());
            m_prefillDecodedCount++;

        } else {
            // Disconnect prefill signal
            disconnect(m_Decoder.get(), &VideoDecoder::frameLoaded, this, &FrameController::onPrefillCompleted);
            // Get the first frame and set lastPTS
            FrameData* firstFrame = m_frameQueue.getHeadFrame();
            m_lastPTS = firstFrame->pts();

            // Request upload for the first frame
            emit requestUpload(firstFrame);
        }
    }
}

void FrameController::onTimerTick() {

    FrameData* headFrame = m_frameQueue.getHeadFrame();
    // request render frames uploaded from previous tick (and request upload next frame)
    emit requestRender();
    // request to decode next tail frame
    emit requestDecode(m_frameQueue.getTailFrame());

    // Calculate delta time for next sleep
    int64_t currentPTS = headFrame->pts();
    int64_t deltaPTS = currentPTS - m_lastPTS;
    int64_t deltaMs = av_rescale_q(deltaPTS, m_frameQueue.metaPtr()->timeBase(), AVRational{1, 1000});
    
    // send delta to VideoController
    emit currentDelta(deltaMs, m_index);

    m_lastPTS = currentPTS;
}

// Handle frame decoding error and increment Tail
void FrameController:: onFrameDecoded(bool success){
    if (!success){
        // TODO: What to do if decoding fails?
        ErrorReporter::instance().report("Decoding error occurred", LogLevel::Error);
    }else{
        m_frameQueue.incrementTail();
    }
    
}

void FrameController::onFrameUploaded(bool success) {
    if (!success) {
        // TODO: Handle upload error
        ErrorReporter::instance().report("Frame upload error occurred", LogLevel::Error);
    }else{
        // Increment head to indicate frame is ready for rendering
        m_frameQueue.incrementHead();
    }
}


void FrameController::onFrameRendered(bool success) {
    if (!success) {
        // TODO: Handle rendering error
        ErrorReporter::instance().report("Frame rendering error occurred", LogLevel::Error);
    }else{
        // Increment head to indicate frame is ready for rendering
        emit requestUpload(m_frameQueue.getHeadFrame());
    }
}

void FrameController::onRenderError() {
    // TODO: Handle rendering error
    ErrorReporter::instance().report("Rendering error occurred", LogLevel::Error);
}


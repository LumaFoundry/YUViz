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
    
    // Request & Receive signals for decoding
    connect(this, &FrameController::requestDecode, m_Decoder.get(), &VideoDecoder::loadFrame);
    connect(m_Decoder.get(), &VideoDecoder::frameLoaded, this, &FrameController::onFrameDecoded);
    
    // Request & Receive signals for uploading texture to GPU
    connect(this, &FrameController::requestUpload, m_Renderer.get(), &VideoRenderer::uploadFrame);
    connect(m_Renderer.get(), &VideoRenderer::frameUploaded, this, &FrameController::onFrameUploaded);

    // Request for rendering frames
    connect(this, &FrameController::requestRender, m_Renderer.get(), &VideoRenderer::renderFrame);
    // Error handling for renderer
    connect(m_Renderer.get(), &VideoRenderer::errorOccured, this, &FrameController::onRenderError);
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

    // Fill queue with initial frames
    for (int i = 0; i < m_frameQueue.getSize()/2; ++i) {
        emit requestDecode(m_frameQueue.getTailFrame());
    }

    // Initial Frame
    FrameData* firstFrame = m_frameQueue.getHeadFrame();

    // Record initial frame’s PTS as lastPTS
    m_lastPTS = firstFrame->pts();
    m_nextWakeMs = 0;

    // Upload initial frames to GPU
    emit requestUpload(firstFrame);
}

void FrameController::play(bool resumed) {

    // Safe-guard against multiple play creating multiple threads
    if (m_playing) return;

    m_playing = true;

    m_timer.start();

    // Handle resume logic
    if (resumed) {
        // If resumed, set next wake time to the paused time
        m_nextWakeMs = m_pausedAt;
    }

    AVRational timeBase = m_frameQueue.metaPtr()->timeBase();

    // Launch background thread for timer
    QtConcurrent::run([this, timeBase]() {
        while (m_playing) {

            int64_t sleepTime = std::max<int64_t>(0, m_nextWakeMs - m_timer.elapsed());
            QThread::msleep(sleepTime);

            // Tick the timer after sleeping
            QMetaObject::invokeMethod(this, [this]() {
                this->onTimerTick();
            }, Qt::QueuedConnection);

            // Update nextWake using media timeline
            int64_t currentPTS = m_frameQueue.getHeadFrame()->pts();
            int64_t deltaPTS = currentPTS - m_lastPTS;
            m_lastPTS = currentPTS;
        
            // Calculate delta using TimeBase
            int64_t deltaMs = av_rescale_q(deltaPTS, timeBase, AVRational{1, 1000});
            
            // Handle speed (TODO: and direction)
            deltaMs = std::max<int64_t>(1, deltaMs / m_speed);

            m_nextWakeMs += deltaMs;
        }

    });
}


void FrameController::pause() {
    // Record current time as paused time
    m_pausedAt = m_timer.elapsed();
    m_playing = false;
}

void FrameController::resume() {
    if (m_playing) return;
    play(true);
}


void FrameController::onTimerTick() {

    // request render frames uploadeed from previous tick
    emit requestRender();
    // request to upload current head frame
    emit requestUpload(m_frameQueue.getHeadFrame());
    // request to decode next tail frame
    emit requestDecode(m_frameQueue.getTailFrame());

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

void FrameController::onRenderError() {
    // TODO: Handle rendering error
    ErrorReporter::instance().report("Rendering error occurred", LogLevel::Error);
}









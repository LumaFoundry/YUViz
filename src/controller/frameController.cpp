#include "frameController.h"

FrameController::FrameController(QObject *parent, VideoDecoder* decoder, VideoRenderer* renderer) 
    : QObject(parent),
      m_Decoder(std::unique_ptr<VideoDecoder>(decoder)),
      m_Renderer(std::unique_ptr<VideoRenderer>(renderer)),
      m_frameQueue(m_Decoder->getMetaData())
{

    // Instantiate playback worker
    m_playbackWorker = std::make_unique<PlaybackWorker>();
    m_playbackWorker->moveToThread(&m_timerThread);

    // Initialize decoder and renderer thread
    m_Decoder->moveToThread(&m_decodeThread);
    m_Renderer->moveToThread(&m_renderThread);
    
    // Request & Receive timer ticks and sleep for frame processing
    connect(m_playbackWorker.get(), &PlaybackWorker::tick, this, &FrameController::onTimerTick, Qt::QueuedConnection);
    connect(this, &FrameController::requestNextSleep, m_playbackWorker.get(), &PlaybackWorker::scheduleNext, Qt::QueuedConnection);

    // Request for interface control signals
    connect(this, &FrameController::requestPlaybackStart, m_playbackWorker.get(), &PlaybackWorker::start, Qt::QueuedConnection);
    connect(this, &FrameController::requestPlaybackPause, m_playbackWorker.get(), &PlaybackWorker::pause, Qt::QueuedConnection);
    connect(this, &FrameController::requestPlaybackResume, m_playbackWorker.get(), &PlaybackWorker::resume, Qt::QueuedConnection);
    connect(this, &FrameController::requestPlaybackStep, m_playbackWorker.get(), &PlaybackWorker::step, Qt::QueuedConnection);
    connect(this, &FrameController::requestPlaybackStop, m_playbackWorker.get(), &PlaybackWorker::stop, Qt::QueuedConnection);

    // Request & Receive signals for decoding
    connect(this, &FrameController::requestDecode, m_Decoder.get(), &VideoDecoder::loadFrame, Qt::QueuedConnection);
    connect(m_Decoder.get(), &VideoDecoder::frameLoaded, this, &FrameController::onFrameDecoded, Qt::QueuedConnection);
    
    // Request & Receive signals for uploading texture to GPU
    connect(this, &FrameController::requestUpload, m_Renderer.get(), &VideoRenderer::uploadFrame, Qt::QueuedConnection);
    connect(m_Renderer.get(), &VideoRenderer::frameUploaded, this, &FrameController::onFrameUploaded, Qt::QueuedConnection);

    // Request for rendering frames
    connect(this, &FrameController::requestRender, m_Renderer.get(), &VideoRenderer::renderFrame, Qt::QueuedConnection);
    // Error handling for renderer
    connect(m_Renderer.get(), &VideoRenderer::errorOccured, this, &FrameController::onRenderError, Qt::QueuedConnection);

    // Clean up worker thread when finished
    connect(&m_timerThread, &QThread::finished, m_playbackWorker.get(), &QObject::deleteLater);

}

FrameController::~FrameController(){
    // Ensure threads are stopped before destruction
    m_decodeThread.quit();
    m_renderThread.quit();
    m_decodeThread.wait();
    m_renderThread.wait();
    m_timerThread.quit();
    m_timerThread.wait();

    // Clear unique pointers
    m_Decoder.reset();
    m_Renderer.reset();
    m_playbackWorker.reset();
}

// Start the decode and render threads
void FrameController::start(){
    m_timerThread.start();
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

    // Upload initial frames to GPU
    emit requestUpload(firstFrame);
}


// Slots Definitions

void FrameController::onTimerTick() {

    FrameData* headFrame = m_frameQueue.getHeadFrame();
    // request render frames uploaded from previous tick
    emit requestRender();
    // request to upload current head frame
    emit requestUpload(headFrame);
    // request to decode next tail frame
    emit requestDecode(m_frameQueue.getTailFrame());

    // Calculate delta time for next sleep
    int64_t currentPTS = headFrame->pts();
    int64_t deltaPTS = currentPTS - m_lastPTS;
    int64_t deltaMs = av_rescale_q(deltaPTS, m_frameQueue.metaPtr()->timeBase(), AVRational{1, 1000});
    
    // Handle speed adjustment (TODO: and direction)
    deltaMs = std::max<int64_t>(1, deltaMs / m_speed);

    // request to schedule next sleep
    emit requestNextSleep(deltaMs);

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

void FrameController::onRenderError() {
    // TODO: Handle rendering error
    ErrorReporter::instance().report("Rendering error occurred", LogLevel::Error);
}


// Interface methods for playback control
void FrameController::play(){
    emit requestPlaybackStart();
}

void FrameController::pause() {
    emit requestPlaybackPause();
}

void FrameController::resume() {
    emit requestPlaybackResume();
}

void FrameController::step() {
    emit requestPlaybackStep();
}

void FrameController::stop() {
    emit requestPlaybackStop();
}

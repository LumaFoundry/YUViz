#include "frameController.h"
#include <QDebug>
#include <QThread>

FrameController::FrameController(
        QObject *parent, 
        std::unique_ptr<VideoDecoder> decoder, 
        std::shared_ptr<PlaybackWorker> playbackWorker, 
        VideoWindow* window, 
        int index)
    : QObject(parent),
        m_Decoder(std::move(decoder)),
        m_PlaybackWorker(playbackWorker),
        m_window(window),
        m_frameQueue(m_Decoder->getMetaData()),
        m_index(index)
{

    // qDebug() << "FrameController constructor invoked for index" << m_index;

    // Initialize decoder and renderer thread
    m_decodeThread.start();
    m_Decoder->moveToThread(&m_decodeThread);
    // qDebug() << "Moved decoder to thread" << &m_decodeThread;
    // m_Renderer->moveToThread(&m_renderThread);
    // qDebug() << "Moved renderer to thread" << &m_renderThread;

    // Request & Receive timer ticks and sleep for frame processing
    connect(m_PlaybackWorker.get(), &PlaybackWorker::tick, this, &FrameController::onTimerTick, Qt::AutoConnection);
    // qDebug() << "Connected PlaybackWorker::tick to FrameController::onTimerTick";

    // Request & Receive signals for decoding
    connect(this, &FrameController::requestDecode, m_Decoder.get(), &VideoDecoder::loadFrame, Qt::AutoConnection);
    // qDebug() << "Connected requestDecode to VideoDecoder::loadFrame";
    connect(m_Decoder.get(), &VideoDecoder::frameLoaded, this, &FrameController::onFrameDecoded, Qt::AutoConnection);
    // qDebug() << "Connected VideoDecoder::frameLoaded to FrameController::onFrameDecoded";

    // Request & Receive signals for uploading texture to buffer
    connect(this, &FrameController::requestUpload, m_window, &VideoWindow::uploadFrame, Qt::AutoConnection);
    // qDebug() << "Connected requestUpload to VideoRenderer::uploadFrame";
    connect(m_window, &VideoWindow::batchUploaded, this, &FrameController::onFrameUploaded, Qt::AutoConnection);
    // qDebug() << "Connected VideoRenderer::batchUploaded to FrameController::onFrameUploaded";

    // Request & Receive for uploading to GPU and rendering frames
    connect(this, &FrameController::requestRender, m_window, &VideoWindow::renderFrame, Qt::AutoConnection);
    // qDebug() << "Connected requestRender to VideoRenderer::renderFrame";
    connect(m_window, &VideoWindow::gpuUploaded, this, &FrameController::onFrameRendered, Qt::AutoConnection);
    // qDebug() << "Connected VideoRenderer::gpuUploaded to FrameController::onFrameRendered";
    
    // Error handling for renderer
    connect(m_window, &VideoWindow::errorOccurred, this, &FrameController::onRenderError, Qt::AutoConnection);
    // qDebug() << "Connected VideoRenderer::errorOccurred to FrameController::onRenderError";

}

FrameController::~FrameController(){
    qDebug() << "FrameController destructor for index" << m_index;
    // Ensure threads are stopped before destruction
    m_decodeThread.quit();
    // m_renderThread.quit();
    m_decodeThread.wait();
    // m_renderThread.wait();

    // Clear unique pointers
    m_Decoder.reset();
}

// Start the decode and render threads
void FrameController::start(){
    // qDebug() << "FrameController::start called for index" << m_index;
    // m_decodeThread.start();
    // qDebug() << "Decode thread started:" << m_decodeThread.currentThreadId();
    // m_renderThread.start();
    // qDebug() << "Render thread started:" << m_renderThread.currentThreadId();

    // Temporary connection for prefill
    connect(m_Decoder.get(), &VideoDecoder::frameLoaded, this, &FrameController::onPrefillCompleted, Qt::AutoConnection);
    // qDebug() << "Connected VideoDecoder::frameLoaded to FrameController::onPrefillCompleted";
    
    // Initial request to decode
    // qDebug() << "Emitting initial requestDecode for prefill";
    emit requestDecode(m_frameQueue.getTailFrame());
}


// Slots Definitions

void FrameController::onPrefillCompleted(bool success) {
    // qDebug() << "onPrefillCompleted called for index" << m_index << "success=" << success;
    if (!success) {
        qWarning() << "Prefill error occurred for index" << m_index;
        ErrorReporter::instance().report("Prefill error occurred", LogLevel::Error);
        return;
    } else {
        if (m_prefillDecodedCount < m_frameQueue.getSize()/2) {
            // qDebug() << "Prefill decode request count =" << m_prefillDecodedCount;
            emit requestDecode(m_frameQueue.getTailFrame());
            m_prefillDecodedCount++;

        } else {
            // Disconnect prefill signal
            disconnect(m_Decoder.get(), &VideoDecoder::frameLoaded, this, &FrameController::onPrefillCompleted);
            // qDebug() << "Prefill signal disconnected";
            // Get the first frame and set lastPTS
            FrameData* firstFrame = m_frameQueue.getHeadFrame();
            m_lastPTS = firstFrame->pts();

            // qDebug() << "Requesting upload for first frame PTS =" << m_lastPTS;
            // Request upload for the first frame
            emit requestUpload(firstFrame);
        }
    }
}

void FrameController::onTimerTick() {
    // qDebug() << "onTimerTick for index" << m_index;

    // request render frames uploaded from previous tick (and request upload next frame)
    emit requestRender();
    // request to decode next tail frame
    emit requestDecode(m_frameQueue.getTailFrame());

    // qDebug() << "Emitted requestRender and requestDecode";
}

// Handle frame decoding error and increment Tail
void FrameController:: onFrameDecoded(bool success){
    // qDebug() << "onFrameDecoded called for index" << m_index << "success=" << success;
    if (!success){
        qWarning() << "Decoding error for index" << m_index;
        // TODO: What to do if decoding fails?
        ErrorReporter::instance().report("Decoding error occurred", LogLevel::Error);
    }else{
        m_frameQueue.incrementTail();
        // qDebug() << "Tail frame index incremented";
    }
    
}

void FrameController::onFrameUploaded(bool success) {
    // qDebug() << "onFrameUploaded for index" << m_index << "success=" << success;
    if (!success) {
        qWarning() << "Frame upload error for index" << m_index;
        // TODO: Handle upload error
        ErrorReporter::instance().report("Frame upload error occurred", LogLevel::Error);
    }
}


void FrameController::onFrameRendered(bool success) {
    // qDebug() << "onFrameRendered for index" << m_index << "success=" << success;
    if (!success) {
        qWarning() << "Frame rendering error for index" << m_index;
        // TODO: Handle rendering error
        ErrorReporter::instance().report("Frame rendering error occurred", LogLevel::Error);
    }else{

        FrameData* lastFrame = m_frameQueue.getHeadFrame();

        if (lastFrame->isEndFrame()) {
            // qDebug() << "End of video reached for index" << m_index;
            emit endOfVideo(m_index);
            return;
        }

        // Increment head to indicate frame is ready for rendering
        m_frameQueue.incrementHead();
        // qDebug() << "Head frame index incremented";

        FrameData* headFrame = m_frameQueue.getHeadFrame();

        // Calculate delta time for next sleep
        int64_t currentPTS = headFrame->pts();

        if (currentPTS < 0) {
            qWarning() << "[FC] Invalid PTS (possibly uninitialized frame); skipping";
            return;
        }

        if (currentPTS < m_lastPTS) {
            qWarning() << "[FC] Non-monotonic PTS: current=" << currentPTS << " last=" << m_lastPTS;
            return;
        }

        int64_t deltaPTS = currentPTS - m_lastPTS;
        int64_t deltaMs = av_rescale_q(deltaPTS, m_frameQueue.metaPtr()->timeBase(), AVRational{1, 1000});
        
        // qDebug() << "FC:: Computed deltaMs =" << deltaMs;
        // send delta to VideoController
        emit currentDelta(deltaMs, m_index);

        m_lastPTS = currentPTS;

        // Request upload for the next frame
        // qDebug() << "FC:: Requesting upload for next head frame";
        emit requestUpload(m_frameQueue.getHeadFrame());
    }
}

void FrameController::onRenderError() {
    qWarning() << "onRenderError for index" << m_index;
    // TODO: Handle rendering error
    ErrorReporter::instance().report("Rendering error occurred", LogLevel::Error);
}

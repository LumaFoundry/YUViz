#include "frameController.h"
#include <QDebug>
#include <QThread>

FrameController::FrameController(
        QObject *parent, 
        VideoFileInfo videoFileInfo, 
        int index)
    : QObject(parent),
        m_index(index)
{
    qDebug() << "FrameController constructor invoked for index" << m_index;

    m_Decoder = std::make_unique<VideoDecoder>();
    m_Decoder->setFileName(videoFileInfo.filename.toStdString());
    m_Decoder->setDimensions(videoFileInfo.width, videoFileInfo.height);
    m_Decoder->setFramerate(videoFileInfo.framerate);
    m_Decoder->openFile();

    m_frameMeta = std::make_shared<FrameMeta>(m_Decoder->getMetaData());
    m_frameQueue = std::make_shared<FrameQueue>(m_frameMeta);

    m_Decoder->setFrameQueue(m_frameQueue);

    m_window = videoFileInfo.windowPtr;
    qDebug() << "Created and showed VideoWindow for index" << m_index << "API" << static_cast<int>(videoFileInfo.graphicsApi);

    m_window->initialize(m_frameMeta);

    // Initialize decoder thread
    m_Decoder->moveToThread(&m_decodeThread);
    qDebug() << "Moved decoder to thread" << &m_decodeThread;

    // Request & Receive signals for decoding
    connect(this, &FrameController::requestDecode, m_Decoder.get(), &VideoDecoder::loadFrames, Qt::AutoConnection);
    qDebug() << "Connected requestDecode to VideoDecoder::loadFrames";

    connect(m_Decoder.get(), &VideoDecoder::framesLoaded, this, &FrameController::onFrameDecoded, Qt::AutoConnection);
    qDebug() << "Connected VideoDecoder::framesLoaded to FrameController::onFrameDecoded";

    connect(this, &FrameController::requestSeek, m_Decoder.get(), &VideoDecoder::seek, Qt::AutoConnection);
    qDebug() << "Connected requestSeek to VideoDecoder::seek";

    connect(m_Decoder.get(), &VideoDecoder::frameSeeked, this, &FrameController::onFrameSeeked, Qt::AutoConnection);
    qDebug() << "Connected VideoDecoder::frameSeeked to FrameController::onFrameSeeked";

    // Request & Receive signals for uploading texture to buffer
    connect(this, &FrameController::requestUpload, m_window, &VideoWindow::uploadFrame, Qt::AutoConnection);
    qDebug() << "Connected requestUpload to VideoRenderer::uploadFrame";
    
    connect(m_window->m_renderer, &VideoRenderer::batchIsFull, this, &FrameController::onFrameUploaded, Qt::AutoConnection);
    qDebug() << "Connected VideoRenderer::batchUploaded to FrameController::onFrameUploaded";

    // Request & Receive for uploading to GPU and rendering frames
    connect(this, &FrameController::requestRender, m_window, &VideoWindow::renderFrame, Qt::AutoConnection);
    qDebug() << "Connected requestRender to VideoRenderer::renderFrame";

    connect(this, &FrameController::requestRelease, m_window, &VideoWindow::releaseBatch, Qt::AutoConnection);
    
    connect(m_window->m_renderer, &VideoRenderer::batchIsEmpty, this, &FrameController::onFrameRendered, Qt::AutoConnection);
    qDebug() << "Connected VideoRenderer::gpuUploaded to FrameController::onFrameRendered";
    
    // Error handling for renderer
    connect(m_window->m_renderer, &VideoRenderer::rendererError, this, &FrameController::onRenderError, Qt::AutoConnection);
    qDebug() << "Connected VideoRenderer::errorOccurred to FrameController::onRenderError";

    m_decodeThread.start();
}

FrameController::~FrameController(){
    qDebug() << "FrameController destructor for index" << m_index;
    // Ensure threads are stopped before destruction
    m_decodeThread.quit();
    m_decodeThread.wait();

    // Clear unique pointers
    m_Decoder.reset();
}

AVRational FrameController::getTimeBase() {
    if (m_frameMeta) {
        return m_frameMeta->timeBase();
    } else {
        qWarning() << "FrameMeta is not initialized, returning default time base";
        return AVRational{1, 1}; // Default to 1000 ms
    }
}

// Start the decode and render threads
void FrameController::start(){
    qDebug() << "FrameController::start called for index" << m_index;

    m_prefill = true;
    // Initial request to decode
    emit requestDecode(m_frameQueue->getSize() / 2);
    qDebug() << "Emitting initial requestDecode for prefill";
}

// Slots Definitions
void FrameController::onTimerTick(int64_t pts) {
    qDebug() << "\nonTimerTick for index" << m_index;

    // Render target frame if inside frameQueue
    FrameData* target = m_frameQueue->getHeadFrame(pts);
    if(target){
        requestRender(target);
        qDebug() << "Requested render for frame with PTS" << pts;
        if (target->isEndFrame()){
            qDebug() << "End frame reached for index" << m_index;
            m_endOfVideo = true;
        }else{
            m_endOfVideo = false;
        }
    }else{
        qWarning() << "Cannot render frame" << pts;
    }

    emit requestRelease();

    // Upload future frame if inside frameQueue
    FrameData* future = m_frameQueue->getHeadFrame(pts + 1);
    if(future){
        requestUpload(future);
        qDebug() << "Requested upload for frame with PTS" << (pts + 1);
    }else{
        qWarning() << "Cannot upload frame" << (pts + 1);
    }

    if (!m_endOfVideo){
        // Request to decode more frames if needed
        int frameToFill = m_frameQueue->getEmpty();
        qDebug() << "Frames to fill in queue:" << frameToFill;
        requestDecode(frameToFill);
    }

    qDebug() << "\n";
}

// Handle frame decoding error and increment Tail
void FrameController:: onFrameDecoded(bool success){
    
    if (!success){
        qWarning() << "Decoding error for index" << m_index;
        // TODO: What to do if decoding fails?
        ErrorReporter::instance().report("Decoding error occurred", LogLevel::Error);
    }

    if (m_prefill){
        qDebug() << "Prefill completed for index" << m_index;
        // Assume first frame has pts 0 and upload to buffer
        requestUpload(m_frameQueue->getHeadFrame(0));
    }

}

void FrameController::onFrameUploaded() {

    if (m_prefill){
        m_prefill = false;
        emit ready(m_index);
    }

    if (m_seeking != -1){
        qDebug() << "FrameController::requesting render after seeked frame uploaded";
        emit requestRender(m_frameQueue->getHeadFrame(m_seeking));
    }

}


void FrameController::onFrameRendered() {
    if (m_endOfVideo) {
        qDebug() << "FrameController::End of frame is rendered for index " << m_index;
        emit endOfVideo(m_index);
        return;
    }

    if (m_seeking != -1){
        qDebug() << "FrameController::Seeked frame is rendered";
        m_seeking = -1; // Reset seeking after rendering
    }
}

void FrameController::onSeek(int64_t pts) {
    qDebug() << "\n Seeking to " << pts << " for index" << m_index;
    // Check if frameQueue has the frame
    FrameData* frame = m_frameQueue->getHeadFrame(pts);
    m_seeking = pts;

    if (!frame){
        qDebug() << "Frame not found in queue, requesting decode for seek pts" << pts;
        emit requestSeek(pts);
    }else{
        qDebug() << "Frame found in queue, requesting upload for seek pts" << pts;
        emit requestUpload(frame);
    }

    qDebug() << "\n";
}


void FrameController::onFrameSeeked(int64_t pts) {
    qDebug() << "FrameController::onFrameSeeked called for index" << m_index << "with PTS" << pts;
    
    FrameData* frameSeeked = m_frameQueue->getHeadFrame(pts);
    if(!frameSeeked){
        qDebug() << "Frame is not seeked";
    }
    requestUpload(frameSeeked);
}


void FrameController::onRenderError() {
    qWarning() << "onRenderError for index" << m_index;
    // TODO: Handle rendering error
    ErrorReporter::instance().report("Rendering error occurred", LogLevel::Error);
}

int FrameController::totalFrames() {
    return m_frameMeta->totalFrames();
}

int64_t FrameController::getDuration(){
    return m_frameMeta->duration();
}
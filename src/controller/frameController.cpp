#include "frameController.h"
#include <QDebug>
#include <QThread>
#include "utils/appConfig.h"

FrameController::FrameController(QObject* parent, VideoFileInfo videoFileInfo, int index) :
    QObject(parent),
    m_index(index) {
    qDebug() << "FrameController constructor invoked for index" << m_index;

    m_Decoder = std::make_unique<VideoDecoder>();
    m_Decoder->setFileName(videoFileInfo.filename.toStdString());
    m_Decoder->setDimensions(videoFileInfo.width, videoFileInfo.height);
    m_Decoder->setFramerate(videoFileInfo.framerate);
    m_Decoder->setFormat(videoFileInfo.pixelFormat);
    m_Decoder->openFile();

    m_frameMeta = std::make_shared<FrameMeta>(m_Decoder->getMetaData());
    m_frameQueue = std::make_shared<FrameQueue>(m_frameMeta, AppConfig::instance().getQueueSize());

    m_Decoder->setFrameQueue(m_frameQueue);

    m_window = videoFileInfo.windowPtr;
    qDebug() << "Created and showed VideoWindow for index" << m_index;

    m_window->initialize(m_frameMeta);

    // Initialize decoder thread

    m_Decoder->moveToThread(&m_decodeThread);
    qDebug() << "Moved decoder to thread" << &m_decodeThread;

    // Request & Receive signals for decoding
    connect(this, &FrameController::requestDecode, m_Decoder.get(), &VideoDecoder::loadFrames, Qt::AutoConnection);
    // qDebug() << "Connected requestDecode to VideoDecoder::loadFrames";

    connect(m_Decoder.get(), &VideoDecoder::framesLoaded, this, &FrameController::onFrameDecoded, Qt::AutoConnection);
    // qDebug() << "Connected VideoDecoder::framesLoaded to FrameController::onFrameDecoded";

    connect(this, &FrameController::requestSeek, m_Decoder.get(), &VideoDecoder::seek, Qt::AutoConnection);
    // qDebug() << "Connected requestSeek to VideoDecoder::seek";

    connect(m_Decoder.get(), &VideoDecoder::frameSeeked, this, &FrameController::onFrameSeeked, Qt::AutoConnection);
    // qDebug() << "Connected VideoDecoder::frameSeeked to FrameController::onFrameSeeked";

    // Request & Receive signals for uploading texture to buffer
    connect(this, &FrameController::requestUpload, m_window, &VideoWindow::uploadFrame, Qt::AutoConnection);
    // qDebug() << "Connected requestUpload to VideoRenderer::uploadFrame";

    connect(
        m_window->m_renderer, &VideoRenderer::batchIsFull, this, &FrameController::onFrameUploaded, Qt::AutoConnection);
    // qDebug() << "Connected VideoRenderer::batchUploaded to FrameController::onFrameUploaded";

    // Request & Receive for uploading to GPU and rendering frames
    connect(this, &FrameController::requestRender, m_window, &VideoWindow::renderFrame, Qt::AutoConnection);
    // qDebug() << "Connected requestRender to VideoRenderer::renderFrame";

    connect(m_window->m_renderer,
            &VideoRenderer::batchIsEmpty,
            this,
            &FrameController::onFrameRendered,
            Qt::AutoConnection);
    // qDebug() << "Connected VideoRenderer::gpuUploaded to FrameController::onFrameRendered";

    // Error handling for renderer
    connect(
        m_window->m_renderer, &VideoRenderer::rendererError, this, &FrameController::onRenderError, Qt::AutoConnection);
    // qDebug() << "Connected VideoRenderer::errorOccurred to FrameController::onRenderError";

    m_decodeThread.start();
}

FrameController::~FrameController() {
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
void FrameController::start() {
    // qDebug() << "FrameController::start called for index" << m_index;

    m_prefill = true;
    // Initial request to decode
    emit requestDecode(m_frameQueue->getSize() / 2, 1);
    // qDebug() << "Emitting initial requestDecode for prefill";
}

// Slots Definitions
void FrameController::onTimerTick(int64_t pts, int direction) {
    // qDebug() << "onTimerTick with pts" << pts << " for index" << m_index;

    // Update VideoWindow with current frame info
    AVRational timeBase = getTimeBase();
    double currentTimeMs = pts * av_q2d(timeBase) * 1000.0;
    m_window->updateFrameInfo(static_cast<int>(pts), currentTimeMs);

    m_direction = direction;

    // Render target frame if inside frameQueue
    FrameData* target = m_frameQueue->getHeadFrame(pts);
    if (target) {
        m_lastPTS = pts;
        // qDebug() << "Requested render for frame with PTS" << pts;
        if (target->isEndFrame()) {
            // qDebug() << "End frame = True set by" << target->pts();
            m_endOfVideo = true;
        } else if (m_endOfVideo) {
            // qDebug() << "End frame = False set by" << target->pts();
            m_endOfVideo = false;
        }
        emit requestRender(m_index);
        emit endOfVideo(m_endOfVideo, m_index);
    } else {
        qWarning() << "Cannot render frame" << pts;
    }

    // Upload future frame if inside frameQueue
    int64_t futurePts = pts + 1 * direction;
    if (futurePts < 0) {
        qWarning() << "Future PTS is negative, cannot upload frame";
        emit endOfVideo(false, m_index);
        return;
    }
    FrameData* future = m_frameQueue->getHeadFrame(futurePts);
    if (future) {
        emit requestUpload(future, m_index);
        // qDebug() << "Requested upload for frame with PTS" << (pts + 1 * direction);
    } else {
        qWarning() << "Cannot upload frame" << (pts + 1 * direction);
    }

    if (!m_endOfVideo) {
        // Request to decode more frames if needed
        int framesToFill = m_frameQueue->getEmpty(direction);
        // qDebug() << "Frames to fill in queue:" << framesToFill;

        emit requestDecode(framesToFill, direction);
    }

    qDebug() << "\n";
}

void FrameController::onTimerStep(int64_t pts, int direction) {
    // qDebug() << "onTimerStep with pts" << pts << " for index" << m_index;

    // Safe guard
    if (pts < 0) {
        qWarning() << "Negative PTS, cannot upload frame";
        emit endOfVideo(true, m_index);
        return;
    }

    // Update VideoWindow with current frame info
    AVRational timeBase = getTimeBase();
    double currentTimeMs = pts * av_q2d(timeBase) * 1000.0;
    m_window->updateFrameInfo(static_cast<int>(pts), currentTimeMs);

    m_stepping = pts;

    // Upload requested Frame
    FrameData* target = m_frameQueue->getHeadFrame(pts);
    if (target) {
        // qDebug() << "Requested upload for frame with PTS" << pts;
        emit requestUpload(target, m_index);
    } else {
        if (direction == 1) {
            emit requestSeek(pts, m_frameQueue->getSize() / 2);
        } else {
            // Safe guard for negative PTS
            int64_t targetPts = std::max(pts - m_frameQueue->getSize() / 2, int64_t(0));
            emit requestSeek(targetPts, m_frameQueue->getSize() / 2);
        }
    }

    qDebug() << "\n";

    m_direction = direction;
}

// Handle frame decoding error and increment Tail
void FrameController::onFrameDecoded(bool success) {
    if (!success) {
        qWarning() << "Decoding error for index" << m_index;
        // TODO: What to do if decoding fails?
        ErrorReporter::instance().report("Decoding error occurred", LogLevel::Error);
    }

    if (m_prefill) {
        // qDebug() << "Prefill completed for index" << m_index;
        // Assume first frame has pts 0 and upload to buffer
        emit requestUpload(m_frameQueue->getHeadFrame(0), m_index);
    }
}

void FrameController::onFrameUploaded() {
    if (m_prefill) {
        m_prefill = false;
        m_window->syncColorSpaceMenu();
        emit requestRender(m_index);
        emit ready(m_index);
    }

    if (m_seeking != -1) {
        // qDebug() << "FrameController::requesting render after seeked frame uploaded";
        FrameData* target = m_frameQueue->getHeadFrame(m_seeking);
        m_lastPTS = m_seeking;
        if (target->isEndFrame()) {
            // qDebug() << "End frame reached after seeking, setting endOfVideo to true";
            m_endOfVideo = true;
        } else if (m_endOfVideo) {
            m_endOfVideo = false;
        }
        emit requestRender(m_index);
    }

    if (m_stepping != -1) {
        // qDebug() << "FrameController::Stepping frame is rendered";
        FrameData* target = m_frameQueue->getHeadFrame(m_stepping);
        m_lastPTS = m_stepping;
        if (target->isEndFrame()) {
            // qDebug() << "End frame reached after stepping, setting endOfVideo to true";
            m_endOfVideo = true;
        } else if (m_endOfVideo) {
            m_endOfVideo = false;
        }

        emit requestRender(m_index);
    }
}

void FrameController::onFrameRendered() {
    // qDebug() << "onFrameRendered";
    if (m_seeking != -1) {
        // qDebug() << "FrameController::Seeked frame is rendered";
        m_seeking = -1; // Reset seeking after rendering
    }

    if (m_stepping != -1) {
        // qDebug() << "FrameController::Stepping frame is rendered";
        m_stepping = -1; // Reset stepping after rendering
    }
}

void FrameController::onSeek(int64_t pts) {
    // qDebug() << "\n Seeking to " << pts << " for index" << m_index;
    // Check if frameQueue has the frame
    FrameData* frame = m_frameQueue->getHeadFrame(pts);
    m_seeking = pts;

    m_endOfVideo = false;

    // Update VideoWindow with current frame info
    AVRational timeBase = getTimeBase();
    double currentTimeMs = pts * av_q2d(timeBase) * 1000.0;
    m_window->updateFrameInfo(static_cast<int>(pts), currentTimeMs);

    if (frame) {
        // qDebug() << "Frame found in queue, requesting upload";
        emit requestUpload(frame, m_index);

        int framesToFill = m_frameQueue->getEmpty(1);
        emit requestDecode(framesToFill, 1);
    } else {
        // qDebug() << "Frame not in queue, requesting seek";
        emit requestSeek(pts, m_frameQueue->getSize() / 2);
    }

    qDebug() << "\n";
}

void FrameController::onFrameSeeked(int64_t pts) {
    // qDebug() << "FrameController::onFrameSeeked called for index" << m_index << "with PTS" << pts;

    int64_t targetPts = pts;
    if (m_stepping != -1) {
        targetPts = m_stepping;
    }

    FrameData* frameSeeked = m_frameQueue->getHeadFrame(targetPts);
    if (!frameSeeked) {
        // qDebug() << "Frame is not seeked";
    } else if (frameSeeked->isEndFrame()) {
        // qDebug() << "End frame reached after seeking, setting endOfVideo to true";
        m_endOfVideo = true;
    } else if (m_endOfVideo) {
        m_endOfVideo = false;
    }

    emit requestUpload(frameSeeked, m_index);
    emit endOfVideo(m_endOfVideo, m_index);
}

void FrameController::onRenderError() {
    qWarning() << "onRenderError for index" << m_index;
    // TODO: Handle rendering error
    ErrorReporter::instance().report("Rendering error occurred", LogLevel::Error);
}

int FrameController::totalFrames() {
    return m_frameMeta->totalFrames();
}

int64_t FrameController::getDuration() {
    return m_frameMeta->duration();
}

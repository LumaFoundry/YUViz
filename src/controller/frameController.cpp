#include "frameController.h"
#include <QThread>
#include "utils/appConfig.h"
#include "utils/debugManager.h"

FrameController::FrameController(QObject* parent, VideoFileInfo videoFileInfo, int index, VideoDecoder* decoder) :
    QObject(parent),
    m_index(index) {
    debug("fc", QString("Constructor invoked for index %1").arg(m_index));

    if (decoder) {
        m_Decoder.reset(decoder);
    } else {
        m_Decoder = std::make_unique<VideoDecoder>();
    }
    m_Decoder->setFileName(videoFileInfo.filename.toStdString());
    m_Decoder->setDimensions(videoFileInfo.width, videoFileInfo.height);
    m_Decoder->setFramerate(videoFileInfo.framerate);
    m_Decoder->setFormat(videoFileInfo.pixelFormat);
    m_Decoder->setForceSoftwareDecoding(videoFileInfo.forceSoftwareDecoding);
    m_Decoder->openFile();

    m_frameMeta = std::make_shared<FrameMeta>(m_Decoder->getMetaData());
    m_frameQueue = std::make_shared<FrameQueue>(m_frameMeta, AppConfig::instance().getQueueSize());

    m_Decoder->setFrameQueue(m_frameQueue);

    m_window = videoFileInfo.windowPtr;
    debug("fc", QString("Created and showed VideoWindow for index %1").arg(m_index));

    m_window->initialize(m_frameMeta);

    // Initialize decoder thread

    m_Decoder->moveToThread(&m_decodeThread);
    debug("fc",
          QString("Moved decoder to thread %1").arg(QString::number(reinterpret_cast<quintptr>(&m_decodeThread))));

    // Request & Receive signals for decoding (cross-thread communication)
    connect(this, &FrameController::requestDecode, m_Decoder.get(), &VideoDecoder::loadFrames, Qt::QueuedConnection);

    connect(m_Decoder.get(), &VideoDecoder::framesLoaded, this, &FrameController::onFrameDecoded, Qt::QueuedConnection);

    connect(this, &FrameController::requestSeek, m_Decoder.get(), &VideoDecoder::seek, Qt::QueuedConnection);

    connect(m_Decoder.get(), &VideoDecoder::frameSeeked, this, &FrameController::onFrameSeeked, Qt::QueuedConnection);

    // Request & Receive signals for uploading texture to buffer (same-thread communication)
    connect(this, &FrameController::requestUpload, m_window, &VideoWindow::uploadFrame, Qt::DirectConnection);

    connect(m_window->m_renderer,
            &VideoRenderer::batchIsFull,
            this,
            &FrameController::onFrameUploaded,
            Qt::QueuedConnection);

    // Request & Receive for uploading to GPU and rendering frames (same-thread communication)
    connect(this, &FrameController::requestRender, m_window, &VideoWindow::renderFrame, Qt::QueuedConnection);

    connect(m_window->m_renderer,
            &VideoRenderer::batchIsEmpty,
            this,
            &FrameController::onFrameRendered,
            Qt::DirectConnection);

    // Error handling for renderer (same-thread communication)
    connect(m_window->m_renderer,
            &VideoRenderer::rendererError,
            this,
            &FrameController::onRenderError,
            Qt::DirectConnection);

    m_decodeThread.start();
}

FrameController::~FrameController() {
    debug("fc", QString("Destructor for index %1").arg(m_index));
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
        warning("fc", "FrameMeta is not initialized, returning default time base");
        return AVRational{1, 1};
    }
}

// Start the decode and render threads
void FrameController::start() {
    debug("fc", QString("start called for index %1").arg(m_index));

    m_prefill = true;
    // Initial request to decode
    emit requestDecode(m_frameQueue->getSize() / 2, 1);
}

// Slots Definitions
void FrameController::onTimerTick(int64_t pts, int direction) {
    debug("fc", QString("onTimerTick with pts %1 for index %2").arg(pts).arg(m_index));

    m_direction = direction;

    // Render target frame if inside frameQueue
    FrameData* target = m_frameQueue->getHeadFrame(pts);
    if (target) {
        m_lastPTS = pts;
        if (target->isEndFrame() && direction == 1) {
            debug("fc", QString("End frame = True set by %1").arg(target->pts()));
            m_endOfVideo = true;
        } else if (m_endOfVideo) {
            debug("fc", QString("End frame = False set by %1").arg(target->pts()));
            m_endOfVideo = false;
        }

        if (m_stalled) {
            clearStall();
        }

        m_ticking = pts;
        debug("fc", QString("Requested render for frame with PTS %1").arg(pts));
        emit requestRender(m_index);
        emit endOfVideo(m_endOfVideo, m_index);

    } else if (!m_prefill && !m_endOfVideo && pts >= 0 && pts < totalFrames()) {
        // If not already stalled, emit signal to VC to pause playback
        if (!m_stalled) {
            m_stalled = true;
            m_waitingPTS = pts;
            debug("fc", QString("Stalled at PTS %1").arg(pts));
            emit decoderStalled(m_index, true);

            if (!m_decodeInProgress) {
                m_decodeInProgress = true;

                if (direction == 1) {
                    emit requestSeek(pts, m_frameQueue->getSize() / 2);
                } else {
                    // Safe guard for negative PTS
                    int64_t targetPts = std::max(pts - m_frameQueue->getSize() / 2, int64_t(0));
                    emit requestSeek(targetPts, m_frameQueue->getSize() / 2);
                }
            }
        }
    } else {
        warning("fc", QString("Cannot render frame %1").arg(pts));
    }
}

void FrameController::onTimerStep(int64_t pts, int direction) {
    debug("fc", QString("onTimerStep with pts %1 for index %2").arg(pts).arg(m_index));

    // Safe guard
    if (pts < 0) {
        warning("fc", "Negative PTS, cannot upload frame");
        emit endOfVideo(true, m_index);
        return;
    }

    m_stepping = pts;

    // Upload requested Frame
    FrameData* target = m_frameQueue->getHeadFrame(pts);
    if (target) {
        debug("fc", QString("Requested upload for frame with PTS %1").arg(pts));
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

    m_direction = direction;
}

// Handle frame decoding error / prefill logic
void FrameController::onFrameDecoded(bool success) {
    if (!success) {
        warning("fc", QString("Decoding error for index %1").arg(m_index));
        ErrorReporter::instance().report("Decoding error occurred", LogLevel::Error);
    }

    // Safe guard for stack calling decode
    m_decodeInProgress = false;

    // Clear stall if decoder caught up
    if (m_stalled && m_waitingPTS != -1) {
        if (m_frameQueue->getHeadFrame(m_waitingPTS)) {
            clearStall();
        } else {
            if (!m_decodeInProgress) {
                m_decodeInProgress = true;
                emit requestSeek(m_waitingPTS, m_frameQueue->getSize() / 2);
            }
        }
    }

    if (m_prefill) {
        debug("fc", QString("Prefill completed for index %1").arg(m_index));
        // Assume first frame has pts 0 and upload to buffer
        FrameData* firstFrame = m_frameQueue->getHeadFrame(0);
        if (firstFrame) {
            emit requestUpload(firstFrame, m_index);
        } else {
            warning("fc", QString("onFrameUploaded: No frame found for PTS 0 at index %1").arg(m_index));
        }
    }
}

void FrameController::onFrameUploaded() {
    debug("fc",
          QString("onFrameUploaded: m_seeking: %1 m_prefill: %2 m_stepping: %3")
              .arg(m_seeking)
              .arg(m_prefill)
              .arg(m_stepping));
    if (m_prefill) {
        m_prefill = false;
        m_window->syncColorSpaceMenu();
        emit requestRender(m_index);
        emit ready(m_index);
    }

    if (m_seeking != -1) {
        FrameData* target = m_frameQueue->getHeadFrame(m_seeking);
        m_lastPTS = m_seeking;
        if (!target) {
            warning("fc",
                    QString("onFrameUploaded - No frame found for PTS %1 at index %2").arg(m_seeking).arg(m_index));
            emit endOfVideo(m_endOfVideo, m_index);
            return;
        }
        if (target->isEndFrame()) {
            debug("fc", "End frame reached after seeking, setting endOfVideo to true");
            m_endOfVideo = true;
        } else if (m_endOfVideo) {
            m_endOfVideo = false;
        }
        debug("fc", QString("Requested render for frame with PTS %1").arg(m_seeking));
        m_seeking = -1; // Reset seeking after upload
        emit seekCompleted(m_index);
        emit requestRender(m_index);
    }

    if (m_stepping != -1) {
        debug("fc", "Stepping frame is rendered");
        FrameData* target = m_frameQueue->getHeadFrame(m_stepping);
        m_lastPTS = m_stepping;
        if (!target) {
            warning("fc",
                    QString("onFrameUploaded - No frame found for PTS %1 at index %2").arg(m_stepping).arg(m_index));
            emit endOfVideo(m_endOfVideo, m_index);
            return;
        }
        if (target->isEndFrame()) {
            debug("fc", "End frame reached after stepping, setting endOfVideo to true");
            m_endOfVideo = true;
        } else if (m_endOfVideo) {
            m_endOfVideo = false;
        }
        debug("fc", QString("Requested render for frame with PTS %1").arg(m_stepping));
        m_stepping = -1; // Reset stepping after upload
        emit requestRender(m_index);
    }

    if (m_ticking != -1) {
        m_ticking = -1;
    }
}

void FrameController::onFrameRendered() {
    debug("fc",
          QString("onFrameRendered: m_ticking: %1 m_seeking: %2 m_stepping: %3")
              .arg(m_ticking)
              .arg(m_seeking)
              .arg(m_stepping));

    if (m_ticking != -1) {
        // Upload future frame if inside frameQueue
        int64_t futurePts = m_ticking + 1 * m_direction;
        if (futurePts < 0) {
            warning("fc", "Future PTS is negative, cannot upload frame");
            emit startOfVideo(m_index);
            m_endOfVideo = false;
            return;
        }
        FrameData* future = m_frameQueue->getHeadFrame(futurePts);
        if (future) {
            debug("fc", QString("Request upload for frame with PTS %1").arg(futurePts));
            m_ticking = -1;
            emit requestUpload(future, m_index);
        } else {
            warning("fc", QString("Cannot upload frame %1").arg(futurePts));
            m_ticking = -1;
        }

        if (!(m_endOfVideo && m_direction == 1) && !(m_ticking == 0 && m_direction == -1) && !m_decodeInProgress) {
            // Request to decode more frames if needed
            int framesToFill = m_frameQueue->getEmpty(m_direction);
            debug("fc",
                  QString("Request decode for %1 in direction %2 decodeInProgress set to true")
                      .arg(framesToFill)
                      .arg(m_direction));
            m_decodeInProgress = true;
            emit requestDecode(framesToFill, m_direction);
        }
    }
}

void FrameController::onSeek(int64_t pts) {
    debug("fc", QString("Seeking to %1 for index %2").arg(pts).arg(m_index));
    // Check if frameQueue has the frame
    FrameData* frame = m_frameQueue->getHeadFrame(pts);
    m_seeking = pts;

    m_endOfVideo = false;

    // Reset any pending decode
    m_decodeInProgress = false;

    // Clear stall on new seek
    clearStall();

    if (frame && !m_frameQueue->isStale(pts)) {
        debug("fc", QString("Frame %1 found in queue, requesting upload").arg(pts));
        emit requestUpload(frame, m_index);

        int direction = (frame->isEndFrame()) ? -1 : 1;
        int framesToFill = m_frameQueue->getEmpty(direction);
        debug("fc", QString("Requesting to fill %1 frames after seeking").arg(framesToFill));
        m_decodeInProgress = true;
        emit requestDecode(framesToFill, direction);

    } else {
        debug("fc", QString("Frame %1 not in queue, requesting seek").arg(pts));
        emit requestSeek(pts, m_frameQueue->getSize() / 2);
    }
}

void FrameController::onFrameSeeked(int64_t pts) {
    debug("fc", QString("onFrameSeeked called for index %1 with PTS %2").arg(m_index).arg(pts));

    int64_t targetPts = pts;
    if (m_stepping != -1) {
        targetPts = m_stepping;
    }

    // Clear stall for internal seeks
    clearStall();

    FrameData* frameSeeked = m_frameQueue->getHeadFrame(targetPts);
    if (!frameSeeked) {
        warning("fc", QString("onFrameSeeked - No frame found for PTS %1 at index %2").arg(targetPts).arg(m_index));
        // Still emit endOfVideo signal to maintain state consistency
        emit endOfVideo(m_endOfVideo, m_index);
        return; // Don't proceed if frame is null
    } else if (frameSeeked->isEndFrame()) {
        debug("fc", "End frame reached after seeking, setting endOfVideo to true");
        m_endOfVideo = true;
    } else if (m_endOfVideo) {
        m_endOfVideo = false;
    }

    emit requestUpload(frameSeeked, m_index);
    emit endOfVideo(m_endOfVideo, m_index);
}

void FrameController::onRenderError() {
    warning("fc", QString("onRenderError for index %1").arg(m_index));
    ErrorReporter::instance().report("Rendering error occurred", LogLevel::Error);
}

int FrameController::totalFrames() {
    return m_frameMeta->totalFrames();
}

int64_t FrameController::getDuration() {
    return m_frameMeta->duration();
}

void FrameController::clearStall() {
    if (!m_stalled)
        return;
    m_stalled = false;
    m_waitingPTS = -1;
    emit decoderStalled(m_index, false);
}
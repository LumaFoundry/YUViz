#include "videoController.h"

VideoController::VideoController(QObject *parent, std::shared_ptr<PlaybackWorker> playbackWorker)
    : QObject(parent),
    m_playbackWorker(playbackWorker),
    frame_controllers_({})
{
    // Connections will be established when adding frame controllers
    connect(this, &VideoController::get_next_tick, m_playbackWorker.get(), &PlaybackWorker::scheduleNext, Qt::QueuedConnection);
}

VideoController::~VideoController() {
    m_playbackWorker.reset();
}

void VideoController::addFrameController(FrameController* frame_controller) {
    frame_controllers_.push_back(frame_controller);
    // Connect each FC's signal to VC's slot
    connect(frame_controller, &FrameController::currentDelta, this, &VideoController::synchroniseFC, Qt::QueuedConnection);

    // Connect each FC's upload signal to VC's slot
    connect(frame_controller->getRenderer(), &VideoRenderer::batchUploaded, this, &VideoController::uploadReady, Qt::QueuedConnection);
}

std::vector<FrameController*> VideoController::getFrameControllers() {
    return frame_controllers_;
}

void VideoController::uploadReady(bool success) {
    if (success) {
        m_readyCount++;
        if (m_readyCount == frame_controllers_.size()) {
            // All frame controllers are ready, start playback
            m_playbackWorker->start();
        }
    } else {
        ErrorReporter::instance().report("Frame upload failed");
    }
}


void VideoController::synchroniseFC(int64_t pts, int index) {
    emit get_next_tick(pts);
}


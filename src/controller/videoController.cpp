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
}

std::vector<FrameController*> VideoController::getFrameControllers() {
    return frame_controllers_;
}

void VideoController::synchroniseFC(int64_t pts, int index) {
    emit get_next_tick(pts);
}


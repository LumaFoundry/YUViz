#include "videoController.h"

VideoController::VideoController(QObject *parent, PlaybackWorker* playbackWorker)
    : QObject(parent),
    m_playbackWorker(std::shared_ptr<PlaybackWorker>(playbackWorker)),
    frame_controllers_({})
{
    // Connections will be established when adding frame controllers
    connect(m_playbackWorker.get(), &PlaybackWorker::scheduleNext, this, &VideoController::synchroniseFC, Qt::QueuedConnection);
    connect(this, &VideoController::get_next_tick, m_playbackWorker.get(), &PlaybackWorker::scheduleNext, Qt::QueuedConnection);
}

VideoController::~VideoController() {
    m_playbackWorker.reset();
}

void VideoController::addFrameController(FrameController* frame_controller) {
    frame_controllers_.push_back(frame_controller);
}

std::vector<FrameController*> VideoController::getFrameControllers() {
    return frame_controllers_;
}

void VideoController::synchroniseFC(int64_t pts, int index) {
    emit get_next_tick(pts);
}


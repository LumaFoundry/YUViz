#include "videoController.h"
#include <QTimer>
#include "utils/debugManager.h"

VideoController::VideoController(QObject* parent,
                                 std::shared_ptr<CompareController> compareController,
                                 std::vector<VideoFileInfo> videoFiles) :
    QObject(parent),
    m_compareController(compareController) {
    debug("vc", QString("Constructor invoked with %1 videoFiles").arg(videoFiles.size()));

    // Creating FC for each video
    for (const auto& videoFile : videoFiles) {
        addVideo(videoFile);
    }

    if (!m_frameControllers.empty()) {
        debug("vc", QString("All FrameControllers created. Total count: %1").arg(m_frameControllers.size()));
        m_timer = std::make_unique<Timer>(nullptr, m_timeBases);
        setUpTimer();
    }
}

VideoController::~VideoController() {
    debug("vc", "Destructor called");

    emit stopTimer();

    m_frameControllers.clear();

    m_timerThread.quit();
    m_timerThread.wait();
}

void VideoController::addVideo(VideoFileInfo videoFile) {

    if (m_timer != nullptr) {
        seekTo(0.0);
        m_timer.reset();
    }

    debug("vc", QString("Setting up FrameController for video: %1 index: %2").arg(videoFile.filename).arg(m_fcIndex));

    debug("vc", QString("Decoder opened file: %1").arg(videoFile.filename));
    auto frameController = std::make_unique<FrameController>(nullptr, videoFile, m_fcIndex);
    debug("vc", QString("Created FrameController for index %1").arg(m_fcIndex));

    // Connect each FC's upload signal to VC's slot
    connect(frameController.get(), &FrameController::ready, this, &VideoController::onReady, Qt::DirectConnection);

    connect(frameController.get(),
            &FrameController::startOfVideo,
            this,
            &VideoController::onFCStartOfVideo,
            Qt::DirectConnection);

    connect(frameController.get(),
            &FrameController::endOfVideo,
            this,
            &VideoController::onFCEndOfVideo,
            Qt::DirectConnection);

    connect(frameController.get(),
            &FrameController::seekCompleted,
            this,
            &VideoController::onSeekCompleted,
            Qt::DirectConnection);

    connect(frameController.get(),
            &FrameController::decoderStalled,
            this,
            &VideoController::onDecoderStalled,
            Qt::DirectConnection);

    m_timeBases.push_back(frameController->getTimeBase());

    // Get the max duration & totalFrames from all FC (in theory they should all be the same)
    m_duration = std::max(m_duration, frameController->getDuration());
    emit durationChanged();
    m_totalFrames = std::max(m_totalFrames, frameController->totalFrames());
    emit totalFramesChanged();

    m_realEndMs =
        (static_cast<double>(m_totalFrames - 1) / static_cast<double>(m_totalFrames)) * static_cast<double>(m_duration);
    debug("vc", QString("Real end time in ms: %1").arg(m_realEndMs));

    m_frameControllers.push_back(std::move(frameController));
    debug("vc", QString("FrameController count now: %1").arg(m_frameControllers.size()));

    m_timer = std::make_unique<Timer>(nullptr, m_timeBases);
    setUpTimer();
    m_frameControllers[m_fcIndex]->start();
    m_fcIndex++;
    m_realCount++;

    // Reset seeking flag
    m_isSeeking = false;
    emit seekingChanged();
    m_seekedFCs.clear();
}

void VideoController::setUpTimer() {
    m_timer->moveToThread(&m_timerThread);

    // Connect with timer (cross-thread communication)
    connect(m_timer.get(), &Timer::tick, this, &VideoController::onTick, Qt::QueuedConnection);

    connect(m_timer.get(), &Timer::step, this, &VideoController::onStep, Qt::QueuedConnection);

    connect(this, &VideoController::playTimer, m_timer.get(), &Timer::play, Qt::QueuedConnection);

    connect(this, &VideoController::pauseTimer, m_timer.get(), &Timer::pause, Qt::QueuedConnection);

    connect(this, &VideoController::stepForwardTimer, m_timer.get(), &Timer::stepForward, Qt::QueuedConnection);

    connect(this, &VideoController::stepBackwardTimer, m_timer.get(), &Timer::stepBackward, Qt::QueuedConnection);

    connect(this, &VideoController::seekTimer, m_timer.get(), &Timer::seek, Qt::QueuedConnection);

    connect(this, &VideoController::setSpeedTimer, m_timer.get(), &Timer::setSpeed, Qt::QueuedConnection);

    connect(this, &VideoController::playForwardTimer, m_timer.get(), &Timer::playForward, Qt::QueuedConnection);

    connect(this, &VideoController::playBackwardTimer, m_timer.get(), &Timer::playBackward, Qt::QueuedConnection);

    // Start timer thread
    m_timerThread.start();
}

void VideoController::removeVideo(int index) {
    debug("vc", QString("Removing video at index %1").arg(index));

    if (index < 0 || index >= m_frameControllers.size()) {
        warning("vc", QString("Invalid index for removing video: %1").arg(index));
        return;
    }

    // Remove per-FC state to avoid stale membership
    m_readyFCs.remove(index);
    m_startFCs.remove(index);
    m_endFCs.remove(index);
    m_seekedFCs.remove(index);
    m_stalledFCs.remove(index);

    // Remove the FrameController at the specified index
    m_frameControllers[index].reset();
    m_realCount--;

    // Recalculate duration based on remaining videos
    int64_t newDuration = 0;
    for (const auto& fc : m_frameControllers) {
        if (fc) {
            newDuration = std::max(newDuration, fc->getDuration());
        }
    }
    m_duration = newDuration;
    emit durationChanged();

    // If all remaining FCs are ready, keep ready=true, otherwise false
    bool allReady = (m_realCount > 0) ? (m_readyFCs.size() == m_realCount) : false;
    if (m_ready != allReady) {
        m_ready = allReady;
        emit readyChanged();
    }

    // Update buffering flag
    bool buffering = !m_stalledFCs.isEmpty();
    if (buffering != m_isBuffering) {
        m_isBuffering = buffering;
        emit isBufferingChanged();
    }

    seekTo(0.0);
}

void VideoController::start() {
    // Start all FC - IMPORTANT: This must be done after all FCs are added !
    for (auto& fc : m_frameControllers) {
        if (fc) {
            debug("vc", QString("Starting FrameController with index: %1").arg(fc->m_index));
            fc->start();
        }
    }
}

void VideoController::onTick(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs) {
    // Update VC-local property and notify QML
    m_currentTimeMs = playingTimeMs;
    emit currentTimeMsChanged();
    for (size_t i = 0; i < m_frameControllers.size(); ++i) {
        if (m_frameControllers[i] && update[i]) {
            debug("vc", QString("Emitted onTimerTick for FrameController index %1 with PTS %2").arg(i).arg(pts[i]));
            m_frameControllers[i]->onTimerTick(pts[i], m_direction);
        }
    }
}

void VideoController::onStep(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs) {
    debug("vc", QString("Step Direction: %1").arg(m_direction));
    // Update VC-local property and notify QML
    m_currentTimeMs = playingTimeMs;
    emit currentTimeMsChanged();

    for (size_t i = 0; i < m_frameControllers.size(); ++i) {
        if (update[i] && m_frameControllers[i]) {
            m_frameControllers[i]->onTimerStep(pts[i], m_direction);
            debug("vc", QString("Emitted onTimerStep for FrameController index %1 with PTS %2").arg(i).arg(pts[i]));
        }
    }
}

void VideoController::onReady(int index) {
    m_readyFCs.insert(index);

    debug("vc", QString("Ready count = %1/%2").arg(m_readyFCs.size()).arg(m_frameControllers.size()));

    if (m_realCount > 0 && m_readyFCs.size() == m_realCount) {
        if (!m_ready) {
            debug("vc", "Ready = true");
            m_ready = true;
            emit readyChanged();
        }
    }
}

void VideoController::onFCStartOfVideo(int index) {
    m_startFCs.insert(index);

    if (m_startFCs.size() == m_realCount && m_realCount > 0) {
        // All FCs reported start
        m_startFCs.clear();
        m_reachedEnd = false;
        m_direction = 1;
        m_uiDirection = 1;
        pause();
        emit directionChanged();
    }
}

void VideoController::onFCEndOfVideo(bool end, int index) {
    // Handle end of video for specific FC

    if (end) {
        debug("vc", QString("FrameController with index %1 reached end of video").arg(index));
        m_endFCs.insert(index);
    } else {
        m_endFCs.remove(index);
    }

    if (m_endFCs.size() == m_realCount && m_realCount > 0) {
        debug("vc", "All FrameControllers reached end of video, stopping playback");

        // Update UI
        m_currentTimeMs = m_duration;
        emit currentTimeMsChanged();

        m_reachedEnd = true;
        pause();

        // Reset end tracking for next run
        m_endFCs.clear();
    }
}

// Interface slots / signals
void VideoController::play() {

    if (!m_ready || m_isSeeking || m_isBuffering) {
        return;
    }

    m_direction = m_uiDirection;

    if (m_reachedEnd && m_timer->getStatus() == Status::Paused) {
        debug("vc", "Restarting playback from beginning");
        m_reachedEnd = false;
        m_pendingPlay = true;
        emit directionChanged();

        if (m_direction == 1) {
            m_direction = 1;
            m_uiDirection = 1;
            seekTo(0.0);

        } else {
            m_direction = -1;
            m_uiDirection = -1;
            seekTo(m_realEndMs);
        }
        // Return early instead of immediately play
        return;
    }

    m_isPlaying = true;
    emit isPlayingChanged();

    if (m_direction == 1) {
        emit playForwardTimer();
    } else {
        emit playBackwardTimer();
    }
}

void VideoController::pause() {
    m_isPlaying = false;
    emit isPlayingChanged();
    emit pauseTimer();
}

void VideoController::stepForward() {

    // Prevent stepping forward during seeking or if not ready
    if (!m_ready || m_isSeeking) {
        return;
    }

    if (m_timer->getStatus() == Status::Playing) {
        debug("vc", "Step forward requested while playing, pausing first");
        pause();
    }

    // Prevent stepping forward if we've already reached the end
    if (m_reachedEnd) {
        debug("vc", "Already at end of video, cannot step forward");
        return;
    }

    m_direction = 1;
    m_reachedEnd = false;
    debug("vc", "Step forward requested");
    emit stepForwardTimer();
}

void VideoController::stepBackward() {

    // Prevent stepping backward during seeking or if not ready
    if (!m_ready || m_isSeeking) {
        return;
    }

    if (m_timer->getStatus() == Status::Playing) {
        debug("vc", "Step backward requested while playing, pausing first");
        pause();
    }

    // Reset reachedEnd state if we are stepping backward
    if (m_reachedEnd) {
        m_reachedEnd = false;
    }

    m_direction = -1;
    debug("vc", "Step backward requested");
    emit stepBackwardTimer();
}

void VideoController::togglePlayPause() {
    debug("vc", "togglePlayPause called");

    if (m_timer->getStatus() == Status::Playing) {
        debug("vc", "Pausing playback");
        pause();
    } else if (m_timer->getStatus() == Status::Paused) {
        debug("vc", "Resuming playback");
        play();
    }
}

void VideoController::seekTo(double timeMs) {

    // disable seeking if not ready
    if (!m_ready) {
        return;
    }

    // Pause the timer
    if (m_timer->getStatus() == Status::Playing) {
        debug("vc", "Pausing playback");
        pause();
    }

    m_isSeeking = true;
    emit seekingChanged();
    m_seekedFCs.clear();

    m_reachedEnd = false;

    m_currentTimeMs = timeMs;
    emit currentTimeMsChanged();

    std::vector<int64_t> seekPts;

    double target = (timeMs >= m_duration) ? m_realEndMs : timeMs;

    for (size_t i = 0; i < m_frameControllers.size(); ++i) {
        // Convert timeMs to PTS using the FC's timebase
        AVRational timebase = m_timeBases[i];
        int64_t pts = llrint((target / 1000.0) / av_q2d(timebase));

        if (m_frameControllers[i]) {
            // Call seek on the FC
            debug("vc",
                  QString("Seeking FrameController index %1 to PTS %2").arg(m_frameControllers[i]->m_index).arg(pts));
            m_frameControllers[i]->onSeek(pts);
        }

        seekPts.push_back(pts);
    }

    // send signal to timer to seek
    emit seekTimer(seekPts);
}

void VideoController::jumpToFrame(int pts) {
    // Convert pts to milliseconds to first active FC
    for (const auto& fc : m_frameControllers) {
        if (fc) {
            // Convert PTS to milliseconds using the FC's timebase
            AVRational timebase = fc->getTimeBase();
            double timeMs = pts * av_q2d(timebase) * 1000.0;
            m_currentTimeMs = timeMs;
            break;
        }
    }
    seekTo(m_currentTimeMs);
}

qint64 VideoController::duration() const {
    debug("vc", QString("Returning duration %1").arg(m_duration));
    return m_duration;
}

bool VideoController::isPlaying() const {
    debug("vc", QString("Returning isPlaying %1").arg(m_isPlaying));
    return m_isPlaying;
}

void VideoController::setSpeed(float speed) {
    debug("vc", QString("Setting playback speed to %1").arg(speed));

    // Convert float to AVRational
    AVRational speedRational;

    speedRational.num = speed * 1000;
    speedRational.den = 1000;

    debug("vc", QString("emitting speed %1/%2 to timer").arg(speedRational.num).arg(speedRational.den));
    emit setSpeedTimer(speedRational);
}

void VideoController::toggleDirection() {
    if (m_uiDirection == 1) {
        m_uiDirection = -1;
        m_direction = -1;
        debug("vc", "Toggled direction to backward");
    } else {
        m_uiDirection = 1;
        m_direction = 1;
        debug("vc", "Toggled direction to forward");
    }

    emit directionChanged();

    if (m_isPlaying) {
        play();
    }
}

void VideoController::setDiffMode(bool diffMode, int id1, int id2) {

    pause();

    m_diffMode = diffMode;

    if (id1 < 0 || id1 >= m_frameControllers.size()) {
        warning("vc", QString("Invalid video ID1 for diff mode: %1").arg(id1));
        return;
    }

    if (id2 < 0 || id2 >= m_frameControllers.size()) {
        warning("vc", QString("Invalid video ID2 for diff mode: %1").arg(id2));
        return;
    }

    if (m_diffMode) {
        m_compareController->setVideoIds(id1, id2);
        m_compareController->setMetadata(m_frameControllers[id1]->getFrameMeta(),
                                         m_frameControllers[id2]->getFrameMeta(),
                                         m_frameControllers[id1]->getFrameQueue(),
                                         m_frameControllers[id2]->getFrameQueue());

        debug("vc", QString("Connecting signals to compare controller"));
        // Connect compare controller to FCs (same-thread communication)
        connect(m_frameControllers[id1].get(),
                &FrameController::requestUpload,
                m_compareController.get(),
                &CompareController::onReceiveFrame,
                Qt::DirectConnection);

        connect(m_frameControllers[id1].get(),
                &FrameController::requestRender,
                m_compareController.get(),
                &CompareController::onRequestRender,
                Qt::DirectConnection);

        connect(m_frameControllers[id2].get(),
                &FrameController::requestUpload,
                m_compareController.get(),
                &CompareController::onReceiveFrame,
                Qt::DirectConnection);

        connect(m_frameControllers[id2].get(),
                &FrameController::requestRender,
                m_compareController.get(),
                &CompareController::onRequestRender,
                Qt::DirectConnection);

        // Defer the seek operation using QTimer
        QTimer::singleShot(150, this, [this]() { seekTo(m_currentTimeMs); });

    } else {

        debug("vc", QString("Diff Mode off"));

        if (m_frameControllers[id1]) {
            debug("vc", QString("Disconnecting FC %1 from compare controller").arg(id1));
            // Disconnect when diff mode is disabled
            disconnect(m_frameControllers[id1].get(),
                       &FrameController::requestUpload,
                       m_compareController.get(),
                       &CompareController::onReceiveFrame);

            disconnect(m_frameControllers[id1].get(),
                       &FrameController::requestRender,
                       m_compareController.get(),
                       &CompareController::onRequestRender);
        }

        if (m_frameControllers[id2]) {
            debug("vc", QString("Disconnecting FC %1 from compare controller").arg(id2));
            // Disconnect when diff mode is disabled
            disconnect(m_frameControllers[id2].get(),
                       &FrameController::requestUpload,
                       m_compareController.get(),
                       &CompareController::onReceiveFrame);

            disconnect(m_frameControllers[id2].get(),
                       &FrameController::requestRender,
                       m_compareController.get(),
                       &CompareController::onRequestRender);
        }

        m_compareController->setVideoIds(-1, -1);
        m_compareController->setMetadata(nullptr, nullptr, nullptr, nullptr);
        m_compareController->setDiffWindow(nullptr);
    }
}

void VideoController::onSeekCompleted(int index) {
    m_seekedFCs.insert(index);
    debug("vc", QString("Seek completed for FC %1 (%2/%3)").arg(index).arg(m_seekedFCs.size()).arg(m_realCount));

    if (m_seekedFCs.size() == m_realCount && m_realCount > 0) {
        // All FrameControllers have completed seeking
        m_isSeeking = false;
        emit seekingChanged();
        debug("vc", "All seeks completed, playback can resume");

        // Play if pending due to end of video
        if (m_pendingPlay) {
            m_pendingPlay = false;
            play();
        }
    }
}

void VideoController::onDecoderStalled(int index, bool stalled) {
    if (stalled) {
        m_stalledFCs.insert(index);
        if (!m_isBuffering) {
            m_isBuffering = true;
            emit isBufferingChanged();
        }
        if (m_timer->getStatus() == Status::Playing) {
            m_wasPlayingWhenStalled = true;
            pause();
        }
    } else {
        m_stalledFCs.remove(index);
        const bool bufferingNow = !m_stalledFCs.isEmpty();
        if (bufferingNow != m_isBuffering) {
            m_isBuffering = bufferingNow;
            emit isBufferingChanged();
        }
        if (!bufferingNow && (m_wasPlayingWhenStalled || m_isPlaying) && m_timer->getStatus() == Status::Paused &&
            !m_isSeeking && !m_reachedEnd) {
            m_wasPlayingWhenStalled = false;
            play();
        }
    }
}
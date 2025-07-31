#include "videoController.h"
#include <QDebug>
#include <QTimer>

VideoController::VideoController(QObject* parent,
                                 std::shared_ptr<CompareController> compareController,
                                 std::vector<VideoFileInfo> videoFiles) :
    QObject(parent),
    m_compareController(compareController) {
    qDebug() << "VideoController constructor invoked with" << videoFiles.size() << "videoFiles";

    // Creating FC for each video
    for (const auto& videoFile : videoFiles) {
        addVideo(videoFile);
    }

    if (!m_frameControllers.empty()) {
        qDebug() << "All FrameControllers created. Total count:" << m_frameControllers.size();
        m_timer = std::make_unique<Timer>(nullptr, m_timeBases);
        setUpTimer();
    }
}

VideoController::~VideoController() {
    qDebug() << "VideoController destructor called";

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

    // qDebug() << "Setting up FrameController for video:" << videoFile.filename << "index:" << m_fcIndex;

    // qDebug() << "Decoder opened file:" << videoFile.filename;
    auto frameController = std::make_unique<FrameController>(nullptr, videoFile, m_fcIndex);
    qDebug() << "Created FrameController for index" << m_fcIndex;

    // Connect each FC's upload signal to VC's slot
    connect(frameController.get(), &FrameController::ready, this, &VideoController::onReady, Qt::AutoConnection);
    // qDebug() << "Connected FrameController::ready to VideoController::onReady";

    connect(frameController.get(),
            &FrameController::endOfVideo,
            this,
            &VideoController::onFCEndOfVideo,
            Qt::AutoConnection);
    // qDebug() << "Connected FrameController::endOfVideo to VideoController::onFCEndOfVideo";

    m_timeBases.push_back(frameController->getTimeBase());

    // Get the max duration from all FC (in theory they should all be the same)
    m_duration = std::max(m_duration, frameController->getDuration());
    emit durationChanged();

    m_frameControllers.push_back(std::move(frameController));
    // qDebug() << "FrameController count now:" << m_frameControllers.size();

    m_timer = std::make_unique<Timer>(nullptr, m_timeBases);
    setUpTimer();
    m_frameControllers[m_fcIndex]->start();
    m_fcIndex++;
    m_realCount++;
}

void VideoController::setUpTimer() {
    m_timer->moveToThread(&m_timerThread);
    // qDebug() << "Move timer to thread" << &m_timerThread;

    // Connect with timer
    connect(m_timer.get(), &Timer::tick, this, &VideoController::onTick, Qt::AutoConnection);
    // qDebug() << "Connected Timer::tick to VideoController::onTick";

    connect(m_timer.get(), &Timer::step, this, &VideoController::onStep, Qt::AutoConnection);
    // qDebug() << "Connected Timer::step to VideoController::onStep";

    connect(this, &VideoController::playTimer, m_timer.get(), &Timer::play, Qt::AutoConnection);
    // qDebug() << "Connected VideoController::playTimer to Timer::play";

    connect(this, &VideoController::pauseTimer, m_timer.get(), &Timer::pause, Qt::AutoConnection);
    // qDebug() << "Connected VideoController::pauseTimer to Timer::pause";

    connect(this, &VideoController::stepForwardTimer, m_timer.get(), &Timer::stepForward, Qt::AutoConnection);
    // qDebug() << "Connected VideoController::stepForwardTimer to Timer::stepForward";

    connect(this, &VideoController::stepBackwardTimer, m_timer.get(), &Timer::stepBackward, Qt::AutoConnection);
    // qDebug() << "Connected VideoController::stepBackwardTimer to Timer::stepBackward";

    connect(this, &VideoController::seekTimer, m_timer.get(), &Timer::seek, Qt::AutoConnection);
    // qDebug() << "Connected VideoController::seekTimer to Timer::seek";

    connect(this, &VideoController::setSpeedTimer, m_timer.get(), &Timer::setSpeed, Qt::AutoConnection);
    // qDebug() << "Connected VideoController::setSpeedTimer to Timer::setSpeed";

    connect(this, &VideoController::playForwardTimer, m_timer.get(), &Timer::playForward, Qt::AutoConnection);
    // qDebug() << "Connected VideoController::playForwardTimer to Timer::playForward";

    connect(this, &VideoController::playBackwardTimer, m_timer.get(), &Timer::playBackward, Qt::AutoConnection);
    // qDebug() << "Connected VideoController::playBackwardTimer to Timer::playBackward";

    // Start timer thread
    m_timerThread.start();
}

void VideoController::removeVideo(int index) {
    qDebug() << "Removing video at index" << index;

    if (index < 0 || index >= m_frameControllers.size()) {
        qWarning() << "Invalid index for removing video:" << index;
        return;
    }

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

    seekTo(0.0);
}

void VideoController::start() {
    // Start all FC - IMPORTANT: This must be done after all FCs are added !
    for (auto& fc : m_frameControllers) {
        if (fc) {
            // qDebug() << "Starting FrameController with index: " << fc->m_index;
            fc->start();
        }
    }
}

void VideoController::onTick(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs) {
    // qDebug() << "VideoController: onTick called";
    // Update VC-local property and notify QML
    m_currentTimeMs = playingTimeMs;
    emit currentTimeMsChanged();
    for (size_t i = 0; i < m_frameControllers.size(); ++i) {
        if (m_frameControllers[i] && update[i]) {
            // qDebug() << "Emitted onTimerTick for FrameController index" << i << "with PTS" << pts[i];
            m_frameControllers[i]->onTimerTick(pts[i], m_direction);
        }
    }
}

void VideoController::onStep(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs) {
    // qDebug() << "VideoController: onTick called";
    // qDebug() << "VideoController::Step Direcetion:" << m_direction;
    // Update VC-local property and notify QML
    m_currentTimeMs = playingTimeMs;
    emit currentTimeMsChanged();

    for (size_t i = 0; i < m_frameControllers.size(); ++i) {
        if (update[i] && m_frameControllers[i]) {
            m_frameControllers[i]->onTimerStep(pts[i], m_direction);
            // qDebug() << "Emitted onTimerStep for FrameController index" << i << "with PTS" << pts[i];
        }
    }
}

void VideoController::onReady(int index) {
    m_readyCount++;
    qDebug() << "Ready count =" << m_readyCount << "/ " << m_frameControllers.size();
    if (m_readyCount == m_frameControllers.size()) {
        // All frame controllers are ready, start playback
        qDebug() << "Ready = true";
        m_ready = true;
    } else {
        qWarning() << "onReady: frame upload failed";
        ErrorReporter::instance().report("Frame upload failed");
    }
}

void VideoController::onFCEndOfVideo(bool end, int index) {
    // Handle end of video for specific FC

    if (end) {
        qDebug() << "VideoController: FrameController with index" << index << "reached end of video";
        m_endCount++;
    } else {
        m_endCount = std::max(0, m_endCount - 1);
    }
    qDebug() << "FC end count =" << m_endCount << "/ " << m_realCount;

    if (m_endCount == m_realCount) {
        // Note: it should actually be m_currentTimeMs == duration
        // But for some reason currentTimeMs does not reach duration
        qDebug() << "CurrentTimeMs" << m_currentTimeMs << "/ Duration" << m_duration;
        if (m_currentTimeMs > 0) {
            qDebug() << "All FrameControllers reached end of video, stopping playback";

            // Update UI
            m_currentTimeMs = m_duration;
            emit currentTimeMsChanged();

            m_reachedEnd = true;
            pause();
            m_endCount = 0;
        } else {
            m_reachedEnd = false;
            m_direction = 1;
            m_uiDirection = 1;
            pause();
            emit directionChanged();
            play();
        }
    }
}

// Interface slots / signals
void VideoController::play() {

    if (!m_ready) {
        return;
    }

    m_currentTimeMs = 0;
    emit currentTimeMsChanged();

    m_direction = m_uiDirection;

    if (m_reachedEnd && m_timer->getStatus() == Status::Paused) {
        // qDebug() << "VideoController::Restarting playback from beginning";
        seekTo(0.0);
        m_reachedEnd = false;
        m_direction = 1;
        m_uiDirection = 1;
        emit directionChanged();
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
    if (m_timer->getStatus() == Status::Playing) {
        // qDebug() << "VideoController: Step forward requested while playing, pausing first";
        pause();
    }

    // Prevent stepping forward if we've already reached the end
    if (m_reachedEnd) {
        qDebug() << "VideoController: Already at end of video, cannot step forward";
        return;
    }

    m_direction = 1;
    m_reachedEnd = false;
    // qDebug() << "VideoController: Step forward requested";
    emit stepForwardTimer();
}

void VideoController::stepBackward() {
    if (m_timer->getStatus() == Status::Playing) {
        // qDebug() << "VideoController: Step backward requested while playing, pausing first";
        pause();
    }

    // Reset reachedEnd state if we are stepping backward
    if (m_reachedEnd) {
        m_reachedEnd = false;
    }

    m_direction = -1;
    // qDebug() << "VideoController: Step backward requested";
    emit stepBackwardTimer();
}

void VideoController::togglePlayPause() {
    // qDebug() << "VideoController:: togglePlayPause called";

    if (m_timer->getStatus() == Status::Playing) {
        // qDebug() << "VideoController: Pausing playback";
        pause();
    } else if (m_timer->getStatus() == Status::Paused) {
        // qDebug() << "VideoController: Resuming playback";
        play();
    }
}

void VideoController::seekTo(double timeMs) {
    // Pause the timer
    if (m_timer->getStatus() == Status::Playing) {
        // qDebug() << "VideoController: Pausing playback";
        pause();
    }

    m_reachedEnd = false;

    m_currentTimeMs = timeMs;
    emit currentTimeMsChanged();

    std::vector<int64_t> seekPts;

    for (size_t i = 0; i < m_frameControllers.size(); ++i) {
        // Convert timeMs to PTS using the FC's timebase
        AVRational timebase = m_timeBases[i];
        int64_t pts = llrint((timeMs / 1000.0) / av_q2d(timebase));

        if (m_frameControllers[i]) {
            // Call seek on the FC
            // qDebug() << "Seeking FrameController index" << fc->m_index << "to PTS" << pts;
            m_frameControllers[i]->onSeek(pts);
        }

        seekPts.push_back(pts);
    }

    // send signal to timer to seek
    emit seekTimer(seekPts);
}

qint64 VideoController::duration() const {
    // qDebug() << "VideoController: Returning duration" << m_duration;
    return m_duration;
}

bool VideoController::isPlaying() const {
    // qDebug() << "VideoController: Returning isPlaying" << m_isPlaying;
    return m_isPlaying;
}

void VideoController::setSpeed(float speed) {
    // qDebug() << "VideoController: Setting playback speed to" << speed;

    // Convert float to AVRational
    AVRational speedRational;

    // Simple and effective approach
    speedRational.num = speed * 1000;
    speedRational.den = 1000;

    // qDebug() << "VideoController: emitting speed " << speedRational.num << "/" << speedRational.den << " to timer";
    // Pass it to the timer
    emit setSpeedTimer(speedRational);
}

void VideoController::toggleDirection() {
    if (m_uiDirection == 1) {
        m_uiDirection = -1;
        m_direction = -1;
        // qDebug() << "VideoController: Toggled direction to backward";
    } else {
        m_uiDirection = 1;
        m_direction = 1;
        // qDebug() << "VideoController: Toggled direction to forward";
    }

    emit directionChanged();

    if (m_isPlaying) {
        play();
    }
}

void VideoController::setDiffMode(bool diffMode, int id1, int id2) {

    m_diffMode = diffMode;

    if (id1 < 0 || id1 >= m_frameControllers.size()) {
        qWarning() << "Invalid video ID1 for diff mode:" << id1;
        return;
    }

    if (id2 < 0 || id2 >= m_frameControllers.size()) {
        qWarning() << "Invalid video ID2 for diff mode:" << id2;
        return;
    }

    if (m_diffMode) {
        m_compareController->setVideoIds(id1, id2);
        m_compareController->setMetadata(m_frameControllers[id1]->getFrameMeta(),
                                         m_frameControllers[id2]->getFrameMeta(),
                                         m_frameControllers[id1]->getFrameQueue(),
                                         m_frameControllers[id2]->getFrameQueue());

        // Connect compare controller to FCs
        connect(m_frameControllers[id1].get(),
                &FrameController::requestUpload,
                m_compareController.get(),
                &CompareController::onReceiveFrame);

        connect(m_frameControllers[id1].get(),
                &FrameController::requestRender,
                m_compareController.get(),
                &CompareController::onRequestRender);

        connect(m_frameControllers[id2].get(),
                &FrameController::requestUpload,
                m_compareController.get(),
                &CompareController::onReceiveFrame);

        connect(m_frameControllers[id2].get(),
                &FrameController::requestRender,
                m_compareController.get(),
                &CompareController::onRequestRender);

        // Defer the seek operation using QTimer
        QTimer::singleShot(100, this, [this]() { seekTo(m_currentTimeMs); });

    } else {

        if (m_frameControllers[id1]) {
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
    }
}
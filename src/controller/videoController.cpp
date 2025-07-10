#include "videoController.h"
#include <QDebug>

VideoController::VideoController(QObject *parent, 
                                 std::vector<VideoFileInfo> videoFiles)
        : QObject(parent)
{
    qDebug() << "VideoController constructor invoked with" << videoFiles.size() << "videoFiles";

    std::vector<AVRational> timeBases;

    // Creating FC for each video
    int fc_index = 0;
    for (const auto& videoFile : videoFiles) {
        qDebug() << "Setting up FrameController for video:" << videoFile.filename << "index:" << fc_index;

        // qDebug() << "Decoder opened file:" << videoFile.filename;    
        auto frameController = std::make_unique<FrameController>(nullptr, videoFile, fc_index);
        qDebug() << "Created FrameController for index" << fc_index;

        // Connect each FC's upload signal to VC's slot
        connect(frameController.get(), &FrameController::ready, this, &VideoController::onReady, Qt::AutoConnection);
        qDebug() << "Connected FrameController::ready to VideoController::onReady";

        connect(frameController.get(), &FrameController::endOfVideo, this, &VideoController::onFCEndOfVideo, Qt::AutoConnection);
        qDebug() << "Connected FrameController::endOfVideo to VideoController::onFCEndOfVideo";

        timeBases.push_back(frameController->getTimeBase());

        // Get the max duration from all FC (in theory they should all be the same)
        m_duration = m_duration > frameController->getDuration() ? m_duration : frameController->getDuration();

        m_frameControllers.push_back(std::move(frameController));
        qDebug() << "FrameController count now:" << m_frameControllers.size();

        
        fc_index++;
    }

    qDebug() << "All FrameControllers created. Total count:" << m_frameControllers.size();
    m_timer = std::make_unique<Timer>(nullptr, timeBases);
    m_timer->moveToThread(&m_timerThread);
    qDebug() << "Move timer to thread" << &m_timerThread;

    // Connect with timer
    connect(m_timer.get(), &Timer::tick, this, &VideoController::onTick, Qt::AutoConnection);
    qDebug() << "Connected Timer::tick to VideoController::onTick";

    connect(this, &VideoController::playTimer, m_timer.get(), &Timer::play, Qt::AutoConnection);
    qDebug() << "Connected VideoController::playTimer to Timer::play";

    connect(this, &VideoController::pauseTimer, m_timer.get(), &Timer::pause, Qt::AutoConnection);
    qDebug() << "Connected VideoController::pauseTimer to Timer::pause";

    connect(this, &VideoController::stepForwardTimer, m_timer.get(), &Timer::stepForward, Qt::AutoConnection);
    qDebug() << "Connected VideoController::stepForwardTimer to Timer::stepForward";

    connect(this, &VideoController::stepBackwardTimer, m_timer.get(), &Timer::stepBackward, Qt::AutoConnection);
    qDebug() << "Connected VideoController::stepBackwardTimer to Timer::stepBackward";

    connect(this, &VideoController::seekTimer, m_timer.get(), &Timer::seek, Qt::AutoConnection);
    qDebug() << "Connected VideoController::seekTimer to Timer::seek";

    // Start timer thread
    m_timerThread.start();
}

VideoController::~VideoController() {
    qDebug() << "VideoController destructor called";

    emit stopTimer();

    m_frameControllers.clear();

    m_timerThread.quit();
    m_timerThread.wait();
}

void VideoController::start(){
    // Start all FC - IMPORTANT: This must be done after all FCs are added !
    for (auto& fc : m_frameControllers) {
        qDebug() << "Starting FrameController with index: " << fc->m_index;
        fc->start();
    }
}


void VideoController::onTick(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs) {
    qDebug() << "VideoController: onTick called";
    qDebug() << "Direcetion:" << m_direction;
    for (size_t i = 0; i < m_frameControllers.size(); ++i) {
        if (update[i]) {
            m_frameControllers[i]->onTimerTick(pts[i], m_direction);
            qDebug() << "Emitted onTimerTick for FrameController index" << i << "with PTS" << pts[i];
        }
    }

    // Update VC-local property and notify QML
    if (playingTimeMs != m_currentTimeMs) {
        m_currentTimeMs = playingTimeMs;
        emit currentTimeMsChanged();
    }
}


void VideoController::onReady(int index) {
    m_readyCount++;
    qDebug() << "Ready count =" << m_readyCount;
    if (m_readyCount == m_frameControllers.size()) {
            // All frame controllers are ready, start playback
            qDebug() << "Starting timer";
            emit playTimer();
    }else {
        qWarning() << "onReady: frame upload failed";
        ErrorReporter::instance().report("Frame upload failed");
    }
}

void VideoController::onFCEndOfVideo(int index) {
    qDebug() << "VideoController: FrameController with index" << index << "reached end of video";
    // Handle end of video for specific FC
    m_endCount++;

    if (m_endCount == m_frameControllers.size()) {
        qDebug() << "All FrameControllers reached end of video, stopping playback";
        emit pauseTimer();
        m_endCount = 0;
    }
}


// Interface slots / signals
void VideoController::play(){
    emit playTimer();
}

void VideoController::pause(){
    emit pauseTimer();
}

void VideoController::stepForward() {
    if(m_timer->getStatus() == Status::Playing){
        qDebug() << "VideoController: Step forward requested while playing, pausing first";
        emit pauseTimer();
    }
    m_direction = 1;
    qDebug() << "VideoController: Step forward requested";
    emit stepForwardTimer();
}

void VideoController::stepBackward() {
    if(m_timer->getStatus() == Status::Playing){
        qDebug() << "VideoController: Step backward requested while playing, pausing first";
        emit pauseTimer();
    }
    m_direction = -1;
    qDebug() << "VideoController: Step backward requested";
    emit stepBackwardTimer();
}

void VideoController::togglePlayPause() {

    qDebug() << "VideoController:: togglePlayPause called";

    if (m_timer->getStatus() == Status::Playing) {
        qDebug() << "VideoController: Pausing playback";
        emit pauseTimer();
    } else if (m_timer->getStatus() == Status::Paused) {
        qDebug() << "VideoController: Resuming playback";
        emit playTimer();
    }
}


void VideoController::seekTo(double timeMs){
    // Pause the timer
    if (m_timer->getStatus() == Status::Playing) {
        qDebug() << "VideoController: Pausing playback";
        emit pauseTimer();
    } 

    m_currentTimeMs = timeMs;
    emit currentTimeMsChanged();

    std::vector<int64_t> seekPts;

    for (auto& fc : m_frameControllers) {
        // Convert timeMs to PTS using the FC's timebase
        AVRational timebase = fc->getTimeBase();
        int64_t pts = llrint((timeMs / 1000.0) / av_q2d(timebase));
        qDebug() << "Seeking FrameController index" << fc->m_index << "to PTS" << pts;
        // Call seek on the FC
        fc->onSeek(pts);
        seekPts.push_back(pts);
    }
    // send signal to timer to seek
    emit seekTimer(seekPts);
}


qint64 VideoController::duration() const {
    // qDebug() << "VideoController: Returning duration" << m_duration;
    return m_duration;
}
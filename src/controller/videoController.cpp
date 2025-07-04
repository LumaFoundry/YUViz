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

        connect(frameController.get(), &FrameController::onTimerTick, this, &VideoController::tickFC, Qt::AutoConnection);
        qDebug() << "Connected FrameController::onTimerTick to VideoController::tickFC";

        connect(frameController.get(), &FrameController::endOfVideo, this, &VideoController::onFCEndOfVideo, Qt::AutoConnection);
        qDebug() << "Connected FrameController::endOfVideo to VideoController::onFCEndOfVideo";

        timeBases.push_back(frameController->getTimeBase());

        m_frameControllers.push_back(std::move(frameController));
        qDebug() << "FrameController count now:" << m_frameControllers.size();

        
        fc_index++;
    }

    qDebug() << "All FrameControllers created. Total count:" << m_frameControllers.size();
    m_timer = std::make_unique<Timer>(nullptr, timeBases);
    m_timer->moveToThread(&m_timerThread);

    // Connect with timer
    connect(m_timer.get(), &Timer::tick, this, &VideoController::onTick, Qt::AutoConnection);
    qDebug() << "Connected Timer::tick to VideoController::onTick";

    connect(this, &VideoController::playTimer, m_timer.get(), &Timer::play, Qt::AutoConnection);
    qDebug() << "Connected VideoController::playTimer to Timer::play";

    connect(this, &VideoController::pauseTimer, m_timer.get(), &Timer::pause, Qt::AutoConnection);
    qDebug() << "Connected VideoController::pauseTimer to Timer::pause";

    // Start timer thread
    m_timerThread.start();

    // Start all FC - IMPORTANT: This must be done after all FCs are added !
    for (auto& fc : m_frameControllers) {
        qDebug() << "Starting FrameController with index: " << fc->m_index;
        fc->start();
    }

}

VideoController::~VideoController() {
    qDebug() << "VideoController destructor called";

    emit stopTimer();

    m_frameControllers.clear();

    m_timerThread.quit();
    m_timerThread.wait();

    m_playbackWorker.reset();
}


void VideoController::onTick(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs) {
    qDebug() << "VideoController: onTick called";

    for (size_t i = 0; i < m_frameControllers.size(); ++i) {
        if (update[i]) {
            emit m_frameControllers[i]->onTimerTick(pts[i]);
            qDebug() << "Emitted onTimerTick for FrameController index" << i << "with PTS" << pts[i];
        }
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
    }
}

void VideoController::togglePlayPause() {
    if (m_playbackWorker->m_playing) {
        qDebug() << "VideoController: Pausing playback";
        emit pauseTimer();
    } else {
        qDebug() << "VideoController: Resuming playback";
        emit playTimer();
    }
}
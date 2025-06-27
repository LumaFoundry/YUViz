#include "videoController.h"
#include <QDebug>

VideoController::VideoController(QObject *parent, 
        std::shared_ptr<PlaybackWorker> playbackWorker,
        std::vector<VideoFileInfo> videoFiles)
        : QObject(parent),
        m_playbackWorker(playbackWorker)
{
    // qDebug() << "VideoController constructor invoked with" << videoFiles.size() << "videoFiles";
    
    // Move playback worker to a dedicated thread
    m_playbackWorker->moveToThread(&m_timerThread);
    // qDebug() << "Moved PlaybackWorker to thread" << &m_timerThread;
    m_timerThread.start();
    // Connect with PlaybackWorker
    connect(this, &VideoController::startPlayback, m_playbackWorker.get(), &PlaybackWorker::start, Qt::QueuedConnection);
    connect(this, &VideoController::stopPlayback, m_playbackWorker.get(), &PlaybackWorker::stop, Qt::QueuedConnection);
    connect(this, &VideoController::pausePlayback, m_playbackWorker.get(), &PlaybackWorker::pause, Qt::QueuedConnection);
    connect(this, &VideoController::resumePlayback, m_playbackWorker.get(), &PlaybackWorker::resume, Qt::QueuedConnection);
    connect(this, &VideoController::get_next_tick, m_playbackWorker.get(), &PlaybackWorker::scheduleNext, Qt::QueuedConnection);
    connect(this, &VideoController::stepPlaybackForward, m_playbackWorker.get(), &PlaybackWorker::step, Qt::QueuedConnection);
    // qDebug() << "Connected get_next_tick to PlaybackWorker::scheduleNext";
    
    // Creating FC for each video
    int fc_index = 0;
    for (const auto& videoFile : videoFiles) {
        qDebug() << "Setting up FrameController for video:" << videoFile.filename << "index:" << fc_index;
        auto decoder = std::make_unique<VideoDecoder>();
        decoder->setFileName(videoFile.filename.toStdString());
        decoder->setDimensions(videoFile.width, videoFile.height);
        decoder->setFramerate(videoFile.framerate);
        decoder->openFile();
        // qDebug() << "Decoder opened file:" << videoFile.filename;
        
        std::shared_ptr<FrameMeta> metaPtr = std::make_shared<FrameMeta>(decoder->getMetaData());
        
        auto windowPtr = std::make_shared<VideoWindow>(nullptr, videoFile.graphicsApi);
        windowPtr->resize(900, 600);
        windowPtr->show();
        qDebug() << "Created and showed VideoWindow for index" << fc_index << "API" << static_cast<int>(videoFile.graphicsApi);

        auto renderer = std::make_unique<VideoRenderer>(nullptr, windowPtr, metaPtr);
        renderer->initialize(videoFile.graphicsApi);
        qDebug() << "Renderer initialized for index" << fc_index;

        auto frameController = std::make_unique<FrameController>(nullptr, std::move(decoder), std::move(renderer), playbackWorker, windowPtr, fc_index);
        qDebug() << "Created FrameController for index" << fc_index;

        // For testing - please remove later
        connect(windowPtr.get(), &VideoWindow::togglePlayPause, this, &VideoController::togglePlayPause);
        connect(windowPtr.get(), &VideoWindow::stepForward, this, &VideoController::stepPlaybackForward);

        // Connect each FC's signal to VC's slot
        connect(frameController.get(), &FrameController::currentDelta, this, &VideoController::synchroniseFC, Qt::AutoConnection);
        qDebug() << "Connected FrameController::currentDelta to VideoController::synchroniseFC";
        // Connect each FC's upload signal to VC's slot
        connect(frameController->getRenderer(), &VideoRenderer::batchUploaded, this, &VideoController::uploadReady, Qt::AutoConnection);
        qDebug() << "Connected VideoRenderer::batchUploaded to VideoController::uploadReady";
        
        connect(frameController.get(), &FrameController::endOfVideo, this, &VideoController::onFCEndOfVideo, Qt::AutoConnection);

        m_frameControllers.push_back(std::move(frameController));
        qDebug() << "FrameController count now:" << m_frameControllers.size();
        
        // For unify event and control to windows
        m_windowPtrs.push_back(windowPtr);
        qDebug() << "WindowPtrs count now:" << m_windowPtrs.size();

        fc_index++;
    }
    qDebug() << "All FrameControllers created. Total count:" << m_frameControllers.size();

    // Start all FC - IMPORTANT: This must be done after all FCs are added !
    for (auto& fc : m_frameControllers) {
        qDebug() << "Starting FrameController with index: " << fc->m_index;
        fc->start();
    }

}

VideoController::~VideoController() {
    qDebug() << "VideoController destructor called";

    emit stopPlayback();

    m_frameControllers.clear();

    m_timerThread.quit();
    m_timerThread.wait();

    m_playbackWorker.reset();
}



void VideoController::uploadReady(bool success) {
    // qDebug() << "uploadReady called with success =" << success;
    if (success) {
        m_readyCount++;
        // qDebug() << "Ready count =" << m_readyCount;
        if (m_readyCount == m_frameControllers.size()) {
            // All frame controllers are ready, start playback
            qDebug() << "Starting timer";
            emit startPlayback();
        }
    } else {
        qWarning() << "uploadReady: frame upload failed";
        ErrorReporter::instance().report("Frame upload failed");
    }
}


void VideoController::synchroniseFC(int64_t delta, int index) {
    // qDebug() << "VC:: synchroniseFC called with delta =" << delta << " index =" << index;
    emit get_next_tick(delta);
}


void VideoController::onFCEndOfVideo(int index) {
    qDebug() << "VideoController: FrameController with index" << index << "reached end of video";
    // Handle end of video for specific FC
    emit stopPlayback();
}

void VideoController::togglePlayPause() {
    if (m_playbackWorker->m_playing) {
        qDebug() << "VideoController: Pausing playback";
        emit pausePlayback();
    } else {
        qDebug() << "VideoController: Resuming playback";
        emit resumePlayback();
    }
}
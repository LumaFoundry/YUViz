#include "videoController.h"
#include <QDebug>

VideoController::VideoController(QObject *parent, 
        std::shared_ptr<PlaybackWorker> playbackWorker,
        std::vector<VideoFileInfo> videoFiles)
        : QObject(parent),
        m_playbackWorker(playbackWorker)
{
    qDebug() << "VideoController constructor invoked with" << videoFiles.size() << "videoFiles";
    
    // Move playback worker to a dedicated thread
    m_playbackWorker->moveToThread(&m_timerThread);
    qDebug() << "Moved PlaybackWorker to thread" << &m_timerThread;
    
    // Connect with PlaybackWorker
    connect(this, &VideoController::get_next_tick, m_playbackWorker.get(), &PlaybackWorker::scheduleNext, Qt::AutoConnection);
    qDebug() << "Connected get_next_tick to PlaybackWorker::scheduleNext";

    // Creating FC for each video
    int fc_index = 0;
    for (const auto& videoFile : videoFiles) {
        qDebug() << "Setting up FrameController for video:" << videoFile.filename << "index:" << fc_index;
        auto decoder = std::make_unique<VideoDecoder>();
        decoder->setFileName(videoFile.filename.toStdString());
        decoder->setDimensions(videoFile.width, videoFile.height);
        decoder->openFile();
        qDebug() << "Decoder opened file:" << videoFile.filename;
        
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

        // Connect each FC's signal to VC's slot
        connect(frameController.get(), &FrameController::currentDelta, this, &VideoController::synchroniseFC, Qt::AutoConnection);
        qDebug() << "Connected FrameController::currentDelta to VideoController::synchroniseFC";
        // Connect each FC's upload signal to VC's slot
        connect(frameController->getRenderer(), &VideoRenderer::batchUploaded, this, &VideoController::uploadReady, Qt::AutoConnection);
        qDebug() << "Connected VideoRenderer::batchUploaded to VideoController::uploadReady";
        
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
    m_playbackWorker.reset();
}


void VideoController::uploadReady(bool success) {
    qDebug() << "uploadReady called with success =" << success;
    if (success) {
        m_readyCount++;
        // qDebug() << "Ready count =" << m_readyCount;
        if (m_readyCount == m_frameControllers.size()) {
            // All frame controllers are ready, start playback
            qDebug() << "Starting timer";
            m_timerThread.start();
            m_playbackWorker->start();
        }
    } else {
        qWarning() << "uploadReady: frame upload failed";
        ErrorReporter::instance().report("Frame upload failed");
    }
}


void VideoController::synchroniseFC(int64_t delta, int index) {
    qDebug() << "VC:: synchroniseFC called with delta =" << delta << " index =" << index;
    emit get_next_tick(delta);
}

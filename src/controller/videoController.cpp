#include "videoController.h"

VideoController::VideoController(QObject *parent, 
        std::shared_ptr<PlaybackWorker> playbackWorker,
        std::vector<VideoFileInfo> videoFiles)
        : QObject(parent),
        m_playbackWorker(playbackWorker)
{
    // Connect with PlaybackWorker
    connect(this, &VideoController::get_next_tick, m_playbackWorker.get(), &PlaybackWorker::scheduleNext, Qt::QueuedConnection);

    // Creating FC for each video
    int fc_index = 0;
    for (const auto& videoFile : videoFiles) {
        auto decoder = std::make_unique<VideoDecoder>();
        decoder->setFileName(videoFile.filename.toStdString());
        decoder->setDimensions(videoFile.width, videoFile.height);
        decoder->openFile();
        
        std::shared_ptr<FrameMeta> metaPtr = std::make_shared<FrameMeta>(decoder->getMetaData());
        
        auto windowPtr = std::make_shared<VideoWindow>(nullptr, videoFile.graphicsApi);
        windowPtr->resize(900, 600);
        windowPtr->show();
        
        auto renderer = std::make_unique<VideoRenderer>(nullptr, windowPtr, metaPtr);
        renderer->initialize(videoFile.graphicsApi);

        auto frameController = std::make_unique<FrameController>(nullptr, std::move(decoder), std::move(renderer), playbackWorker, windowPtr, fc_index);

        // Connect each FC's signal to VC's slot
        connect(frameController.get(), &FrameController::currentDelta, this, &VideoController::synchroniseFC, Qt::QueuedConnection);
        // Connect each FC's upload signal to VC's slot
        connect(frameController->getRenderer(), &VideoRenderer::batchUploaded, this, &VideoController::uploadReady, Qt::QueuedConnection);
        
        m_frameControllers.push_back(std::move(frameController));
        
        // For unify event and control to windows
        m_windowPtrs.push_back(windowPtr);

        fc_index++;
    }

    // Start all FC - IMPORTANT: This must be done after all FCs are added !
    for (auto& fc : m_frameControllers) {
        fc->start();
    }

}

VideoController::~VideoController() {
    m_playbackWorker.reset();
}


void VideoController::uploadReady(bool success) {
    if (success) {
        m_readyCount++;
        if (m_readyCount == m_frameControllers.size()) {
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


#pragma once

#include <QThread>
#include <QElapsedTimer>
#include <QtConcurrent>
#include <QVariant>
#include <map>

#include "frameController.h"
#include "frames/frameQueue.h"
#include "frames/frameMeta.h"
#include "frames/frameData.h"
#include "decoder/videoDecoder.h"
#include "rendering/videoRenderer.h"
#include "utils/errorReporter.h"
#include "controller/playBackWorker.h"

class VideoController : public QObject {
    Q_OBJECT

public:
    VideoController(QObject *parent, std::shared_ptr<PlaybackWorker> playbackWorker);
    ~VideoController();

    void addFrameController(FrameController* controller);
    std::vector<FrameController*> getFrameControllers();

public slots:
    void synchroniseFC(int64_t pts, int index);

signals:
    void get_next_tick(int64_t pts);

private:
    std::shared_ptr<PlaybackWorker> m_playbackWorker;
    std::vector<FrameController*> frame_controllers_;

};


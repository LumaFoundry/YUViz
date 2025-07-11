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
#include "controller/timer.h"
#include "ui/videoWindow.h"
#include "utils/videoFileInfo.h"

class VideoController : public QObject {
    Q_OBJECT

    Q_PROPERTY(qint64 duration READ duration CONSTANT)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(double currentTimeMs READ currentTimeMs NOTIFY currentTimeMsChanged)

public:
    VideoController(QObject *parent, 
                    std::vector<VideoFileInfo> videoFiles = {});
    ~VideoController();
    void start();

    qint64 duration() const;
    bool isPlaying() const;
    bool isPlaying() const;
    double currentTimeMs() const { return m_currentTimeMs; }

public slots:
    void onReady(int index);
    void onFCEndOfVideo(int index);
    void onTick(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs);
    void togglePlayPause();
    void play();
    void pause();
    void stepForward();
    void stepBackward();
    void seekTo(double timeMs);
    void seekTo(double timeMs);
    void setSpeed(float speed);

signals:
    void playTimer();
    void stopTimer();
    void pauseTimer();
    void stepForwardTimer();
    void stepBackwardTimer();
    void tickFC(int64_t pts);
    void seekTimer(std::vector<int64_t> seekPts);
    void setSpeedTimer(AVRational speed);
    void currentTimeMsChanged();
    void isPlayingChanged();
    void isPlayingChanged();

private:
    std::vector<std::unique_ptr<FrameController>> m_frameControllers;

    std::shared_ptr<Timer> m_timer;

    QThread m_timerThread;

    // Ensure all FC have uploaded initial frame before starting timer
    int m_readyCount = 0;

    int m_endCount = 0;

    int64_t m_duration = 0;

    int64_t m_currentTimeMs = 0;

    // 1 for forward, -1 for backward
    int m_direction = 1;

    bool m_isPlaying = false;
    bool m_reachedEnd = false;

};


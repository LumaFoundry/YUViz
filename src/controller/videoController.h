#pragma once

#include <QElapsedTimer>
#include <QThread>
#include <QVariant>
#include <QtConcurrent>
#include <map>

#include "controller/compareController.h"
#include "controller/frameController.h"
#include "controller/timer.h"
#include "decoder/videoDecoder.h"
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "frames/frameQueue.h"
#include "rendering/videoRenderer.h"
#include "ui/videoWindow.h"
#include "utils/errorReporter.h"
#include "utils/videoFileInfo.h"

class VideoController : public QObject {
    Q_OBJECT

    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qint64 totalFrames READ totalFrames NOTIFY totalFramesChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(double currentTimeMs READ currentTimeMs NOTIFY currentTimeMsChanged)
    Q_PROPERTY(bool isForward READ isForward NOTIFY directionChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool isSeeking READ isSeeking NOTIFY seekingChanged)

  public:
    VideoController(QObject* parent,
                    std::shared_ptr<CompareController> compareController,
                    std::vector<VideoFileInfo> videoFiles = {});
    ~VideoController();
    void start();

    qint64 duration() const;
    qint64 totalFrames() const { return m_totalFrames; }
    bool isPlaying() const;
    double currentTimeMs() const { return m_currentTimeMs; }
    bool isForward() const { return m_uiDirection == 1; }
    bool ready() const { return m_ready; }
    bool isSeeking() const { return m_isSeeking; }

    void addVideo(VideoFileInfo videoFileInfo);
    void setUpTimer();

    // void createDiffWindow();

  public slots:
    void onReady(int index);
    void onFCStartOfVideo(int index);
    void onFCEndOfVideo(bool end, int index);
    void onTick(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs);
    void onStep(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs);
    void togglePlayPause();
    void play();
    void pause();
    void stepForward();
    void stepBackward();
    void seekTo(double timeMs);
    void jumpToFrame(int pts);
    void setSpeed(float speed);
    void toggleDirection();
    void removeVideo(int index);
    void setDiffMode(bool diffMode, int id1, int id2);
    void onSeekCompleted(int index);

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
    void playBackwardTimer();
    void playForwardTimer();
    void directionChanged();
    void durationChanged();
    void totalFramesChanged();
    void readyChanged();
    void seekingChanged();

  private:
    std::vector<std::unique_ptr<FrameController>> m_frameControllers;
    std::vector<AVRational> m_timeBases;

    int m_fcIndex = 0;
    int m_realCount = 0;

    std::shared_ptr<Timer> m_timer;

    QThread m_timerThread;

    // Ensure all FC have uploaded initial frame before starting timer
    int m_readyCount = 0;
    bool m_ready = false;

    int m_startCount = 0;
    int m_endCount = 0;

    int64_t m_duration = 0;

    int m_totalFrames = 0;

    int64_t m_currentTimeMs = 0;

    // 1 for forward, -1 for backward
    int m_direction = 1;
    int m_uiDirection = 1;

    bool m_isPlaying = false;
    bool m_reachedEnd = false;

    bool m_diffMode = false;

    bool m_isSeeking = false;
    int m_seekedCount = 0;
    bool m_pendingPlay = false;

    std::shared_ptr<CompareController> m_compareController;
};

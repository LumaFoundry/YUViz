#pragma once

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <chrono>
#include <QThread>
#include <QtConcurrent>


class PlaybackWorker : public QObject {
    Q_OBJECT

public:
    PlaybackWorker(){};
    ~PlaybackWorker(){};

    bool m_playing = false;

public slots:
    void scheduleNext(int64_t deltaMs);
    void start();
    void runPlaybackLoop();
    void stop();
    void pause();
    void resume();
    // void step();

signals:
    void tick(); // tells FrameController to advance a frame

private:

    QMutex m_mutex;
    QWaitCondition m_cond;

    bool m_running = false;

    bool m_singleStep = false;

    int64_t m_nextWakeMs = 0;

    int64_t m_pauseStartMs = 0;
};

#pragma once

#include <QObject>
#include <mutex>
#include <vector>

extern "C" {
#include <libavutil/rational.h>
}

enum Status {
    Playing,
    Paused,
    Seeking
};

enum Direction {
    Forward,
    Backward
};

struct Cache {
    std::vector<int64_t> pts;
    std::vector<bool> update;
    std::vector<AVRational> timestamp;
    int64_t playingTimeMs;
    AVRational wake;
};

class Timer : public QObject {
    Q_OBJECT

  public:
    Timer(QObject* parent, std::vector<AVRational> timebase);
    ~Timer();
    Status getStatus() const;

  public slots:
    void play();
    void pause();
    void playForward();
    void playBackward();
    void stepForward();
    void stepBackward();
    void seek(std::vector<int64_t> seekPts);
    void setSpeed(AVRational speed);

  private slots:
    void loop();

  signals:
    void tick(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs);
    void step(std::vector<int64_t> pts, std::vector<bool> update, int64_t playingTimeMs);

  private:
    std::vector<AVRational> m_timebase;
    size_t m_size;
    std::vector<int64_t> m_pts;
    std::vector<bool> m_update;
    std::vector<AVRational> m_timestamp;
    Status m_status;
    Direction m_direction;
    int64_t m_playingTimeMs;
    AVRational m_speed;
    AVRational m_wake;
    Cache m_cache;
    std::mutex m_mutex;

    void saveCache();
    void restoreCache();
    int64_t forwardNext();
    int64_t backwardNext();
    bool autoPause();
    void forwardPts();
    void backwardPts();
    AVRational forwardWake();
    AVRational backwardWake();
    void forwardUpdate(AVRational next);
    void backwardUpdate(AVRational next);
    AVRational absolute(AVRational avrational);
    int64_t nextDeltaMs(AVRational next, AVRational last);
};
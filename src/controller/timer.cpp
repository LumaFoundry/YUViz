#include "timer.h"
#include <chrono>
#include <thread>
#include <algorithm>
#include <QObject>

extern "C"
{
#include <libavutil/common.h> 
#include <libavutil/mathematics.h>
}

Timer::Timer(QObject* parent, std::vector<AVRational> timebase)
    : QObject(parent)
    , m_timebase(std::move(timebase))
    , m_size(m_timebase.size())
    , m_pts(m_size, 0)
    , m_update(m_size, true)
    , m_timestamp(m_size, AVRational{0, 1})
    , m_status(Status::Paused)
    , m_direction(Direction::Forward)
    , m_playingTimeMs(0)
    , m_speed({1, 1})
    , m_wake({0, 1})
{
    saveCache();
}

Timer::~Timer() = default;

void Timer::saveCache()
{
    m_cache.pts = m_pts;
    m_cache.update = m_update;
    m_cache.timestamp = m_timestamp;
    m_cache.playingTimeMs = m_playingTimeMs;
    m_cache.wake = m_wake;
}

void Timer::restoreCache()
{
    m_pts = m_cache.pts;
    m_update = m_cache.update;
    m_timestamp = m_cache.timestamp;
    m_playingTimeMs = m_cache.playingTimeMs;
    m_wake = m_cache.wake;
}

void Timer::loop()
{
    while(m_status.load() == Status(Playing))
    {
        auto now = std::chrono::steady_clock::now();
        int64_t deltaMs = 0;
        switch (m_direction.load())
        {
        case Direction(Forward):
            deltaMs = forwardNext();
            break;
        case Direction(Backward):
            deltaMs = backwardNext();
            break;
        }
        std::this_thread::sleep_until(now + std::chrono::milliseconds(deltaMs));
    }
}

int64_t Timer::forwardNext()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    emit tick(m_pts, m_update, m_playingTimeMs);
    saveCache();
    forwardPts();
    AVRational next = forwardWake();
    forwardUpdate(next);
    int64_t deltaMs = nextDeltaMs(next, m_wake);
    m_playingTimeMs = av_rescale_q(next.num, AVRational{1000, next.den}, AVRational{1, 1});
    m_wake = next;
    return deltaMs;
}

int64_t Timer::backwardNext()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    emit tick(m_pts, m_update, m_playingTimeMs);
    saveCache();
    if (autoPause())
    {
        return 0;
    }
    backwardPts();
    AVRational next = backwardWake();
    backwardUpdate(next);
    int64_t deltaMs = nextDeltaMs(next, m_wake);
    m_playingTimeMs = av_rescale_q(next.num, AVRational{1000, next.den}, AVRational{1, 1});
    m_wake = next;
    return deltaMs;
}

bool Timer::autoPause()
{
    if (m_wake.num == 0)
    {
        m_status = Status(Paused);
        return true;
    }
    return false;
}

void Timer::forwardPts()
{
    for (size_t i = 0; i < m_size; ++i)
    {
        if (m_update[i] == true)
        {
            m_pts[i] ++;
            m_timestamp[i] = av_mul_q(AVRational{static_cast<int>(m_pts[i]), 1}, m_timebase[i]);
            m_update[i] = false;
        }
    }
}

void Timer::backwardPts()
{
    for (size_t i = 0; i < m_size; ++i)
    {
        if (m_pts[i] > 0)
        {
            m_pts[i] --;
            m_timestamp[i] = av_mul_q(AVRational{static_cast<int>(m_pts[i]), 1}, m_timebase[i]);
            m_update[i] = false;
        }
    }
}

AVRational Timer::forwardWake()
{
    auto min = std::min_element(m_timestamp.begin(), m_timestamp.end(), 
                                [](const AVRational& a, const AVRational& b) 
                                {return av_cmp_q(a, b) < 0;});
    return *min;
}

AVRational Timer::backwardWake()
{
    auto max = std::max_element(m_timestamp.begin(), m_timestamp.end(), 
                                [](const AVRational& a, const AVRational& b) 
                                {return av_cmp_q(a, b) > 0;});
    return *max;
}

void Timer::forwardUpdate(AVRational next)
{
    for (size_t i = 0; i < m_size; ++i)
    {
        if (av_cmp_q(m_timestamp[i], next) == 0)
        {
            m_update[i] = true;
        }
    }
}

void Timer::backwardUpdate(AVRational next)
{
    for (size_t i = 0; i < m_size; ++i)
    {
        if (av_cmp_q(m_timestamp[i], next) < 0)
        {
            m_pts[i] ++;
            m_timestamp[i] = av_mul_q(AVRational{static_cast<int>(m_pts[i]), 1}, m_timebase[i]);
        }
        else 
        {
            m_update[i] = true;
        }
    }
}

AVRational Timer::absolute(AVRational r)
{
    return AVRational{FFABS(r.num), FFABS(r.den)};
}

int64_t Timer::nextDeltaMs(AVRational next, AVRational last)
{
    AVRational delta = av_sub_q(next, last);
    delta = absolute(delta);
    delta = av_div_q(delta, m_speed);
    int64_t deltaTime = av_rescale_q(delta.num, AVRational{1000, delta.den}, AVRational{1, 1});
    return deltaTime;
}

void Timer::play()
{
    if (m_status.load() != Status(Playing))
    {
        m_status = Status(Playing);
        loop();
    }
}

void Timer::pause()
{
    if (m_status.load() == Status(Playing))
    {
        m_status = Status(Paused);
        restoreCache();
    }
}

void Timer::playForward()
{
    if (m_direction.load() == Direction(Backward))
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_direction = Direction(Forward);
        restoreCache();
    };
}

void Timer::playBackward()
{
    if (m_direction.load() == Direction(Forward))
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_direction = Direction(Backward);
        restoreCache();
    };
}

void Timer::stepForward()
{
    if (m_status.load() == Status(Paused))
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        forwardPts();
        AVRational next = forwardWake();
        forwardUpdate(next);
        m_playingTimeMs = av_rescale_q(next.num, AVRational{1000, next.den}, AVRational{1, 1});
        emit tick(m_pts, m_update, m_playingTimeMs);
        m_wake = next;
        saveCache();
    }
}

void Timer::stepBackwork()
{
    if (m_status.load() == Status(Paused) && m_wake.num > 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        backwardPts();
        AVRational next = backwardWake();
        backwardUpdate(next);
        m_playingTimeMs = av_rescale_q(next.num, AVRational{1000, next.den}, AVRational{1, 1});
        emit tick(m_pts, m_update, m_playingTimeMs);
        m_wake = next;
        saveCache();
    }
}

void Timer::seek(int64_t seekMs)
{
    if (m_status.load() != Status(Seeking))
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (size_t i = 0; i < m_size; ++i)
        {
            m_pts[i] = av_rescale_q(seekMs, AVRational{1, 1000}, m_timebase[i]);
            m_timestamp[i] = av_mul_q(AVRational{static_cast<int>(m_pts[i]), 1}, m_timebase[i]);
            m_update[i] = true;
        }
        AVRational next = backwardWake();
        m_playingTimeMs = av_rescale_q(next.num, AVRational{1000, next.den}, AVRational{1, 1});
        m_wake = next;
        saveCache();
    }
}

void Timer::setSpeed(AVRational speed)
{
    m_speed = speed;
}
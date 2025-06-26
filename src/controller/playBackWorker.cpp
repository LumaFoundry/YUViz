#include "playBackWorker.h"
#include <QDebug>
#include <QThread>


void PlaybackWorker::start() {
    qDebug() << "PlaybackWorker::start invoked in thread" << QThread::currentThread();
    m_running = true;
    m_playing = true;
    m_timerStart = std::chrono::steady_clock::now();
    m_nextWakeMs = 0;
    
    // Schedule runPlaybackLoop() to run once, asynchronously
    QtConcurrent::run([this](){
        runPlaybackLoop(); 
    });
}

void PlaybackWorker::runPlaybackLoop() {
    // qDebug() << "PlaybackWorker::runPlaybackLoop entered";
    // qDebug() << "runPlaybackLoop this=" << this << "thread=" << QThread::currentThread();

    while (true) {
        QMutexLocker locker(&m_mutex);
        if (!m_running) break;

        auto now = std::chrono::steady_clock::now();
        int64_t elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_timerStart).count();
        int64_t waitTime = std::max<int64_t>(0, m_nextWakeMs - elapsedMs);
        

        // qDebug() << "[loop] received m_nextWakeMs " << m_nextWakeMs;

        qDebug() << "[loop] waiting for" << waitTime << "ms. elapsed=" << elapsedMs;

        QThread::msleep(waitTime);

        if (!m_running) break;

        qDebug() << "[loop] woke up, elapsed=" << elapsedMs << "\n";

        locker.unlock();
        emit tick();
        locker.relock();
    }

    qDebug() << "PlaybackWorker::runPlaybackLoop exiting";
}

void PlaybackWorker::scheduleNext(int64_t deltaMs) {
    // qDebug() << "scheduleNext this=" << this << "thread=" << QThread::currentThread();
    // qDebug() << "PlaybackWorker::scheduleNext called with deltaMs=" << deltaMs;
    QMutexLocker locker(&m_mutex);
    auto now = std::chrono::steady_clock::now();
    m_nextWakeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_timerStart).count() + deltaMs;

    // qDebug() << "Next wake set to" << m_nextWakeMs;
    m_cond.wakeOne();
    // qDebug() << "cond.wakeOne() called in scheduleNext";
}


void PlaybackWorker::stop() {
    qDebug() << "PlaybackWorker::stop called";
    QMutexLocker locker(&m_mutex);
    m_running = false;
    m_cond.wakeOne(); // Wake up the thread if it's waiting
}

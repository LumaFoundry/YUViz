#include "playBackWorker.h"
#include <QDebug>
#include <QThread>


void PlaybackWorker::start() {
    qDebug() << "PlaybackWorker::start invoked in thread" << QThread::currentThread();
    m_running = true;
    m_playing = true;
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

        // Cap waitTime with small delay to ensure loop doesn't spin too fast
        int64_t waitTime = std::max<int64_t>(1, m_nextWakeMs);

        // qDebug() << "[loop] m_nextWakeMs: " << m_nextWakeMs;

        QThread::msleep(waitTime);

        if (!m_running) break;

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
    m_nextWakeMs = deltaMs;
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

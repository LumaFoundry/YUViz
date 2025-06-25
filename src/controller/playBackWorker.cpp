#include "playBackWorker.h"
#include <QDebug>
#include <QThread>


void PlaybackWorker::start() {
    qDebug() << "PlaybackWorker::start invoked in thread" << QThread::currentThread();
    m_running = true;
    m_playing = true;
    m_timer.start();
    m_nextWakeMs = 0;
    
    // Schedule runPlaybackLoop() to run once, asynchronously
    QtConcurrent::run([this](){
        runPlaybackLoop(); 
    });
}

void PlaybackWorker::runPlaybackLoop() {
    qDebug() << "PlaybackWorker::runPlaybackLoop entered";
    qDebug() << "runPlaybackLoop this=" << this << "thread=" << QThread::currentThread();

    while (true) {
        QMutexLocker locker(&m_mutex);
        if (!m_running) break;

        int64_t waitTime = std::max<int64_t>(0, m_nextWakeMs - m_timer.elapsed());

        qDebug() << "[loop] received m_nextWakeMs " << m_nextWakeMs;

        qDebug() << "[loop] waiting for" << waitTime << "ms. elapsed=" << m_timer.elapsed();

        QThread::msleep(waitTime);

        if (!m_running) break;

        qDebug() << "[loop] woke up, elapsed=" << m_timer.elapsed();

        locker.unlock();
        emit tick();
        locker.relock();
    }

    qDebug() << "PlaybackWorker::runPlaybackLoop exiting";
}

void PlaybackWorker::scheduleNext(int64_t deltaMs) {
    qDebug() << "scheduleNext this=" << this << "thread=" << QThread::currentThread();
    qDebug() << "PlaybackWorker::scheduleNext called with deltaMs=" << deltaMs;
    QMutexLocker locker(&m_mutex);
    m_nextWakeMs = m_timer.elapsed() + deltaMs;
    qDebug() << "Next wake set to" << m_nextWakeMs;
    m_cond.wakeOne();
    // qDebug() << "cond.wakeOne() called in scheduleNext";
}
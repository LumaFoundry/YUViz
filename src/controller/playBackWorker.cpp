#include "playBackWorker.h"
#include <QDebug>
#include <QThread>

void PlaybackWorker::start() {
    QMetaObject::invokeMethod(this, [this]() {
        qDebug() << "PlaybackWorker::start invoked in thread" << QThread::currentThread();
        m_running = true;
        m_playing = true;
        qDebug() << "PlaybackWorker state: running=" << m_running << "playing=" << m_playing;
        m_timer.start();
        m_nextWakeMs = 0;
        qDebug() << "PlaybackWorker timer started and nextWakeMs =" << m_nextWakeMs;
        runPlaybackLoop();
    }, Qt::QueuedConnection);
}


void PlaybackWorker::pause() {
    qDebug() << "PlaybackWorker::pause called";
    QMutexLocker locker(&m_mutex);
    if (!m_playing) {
        qDebug() << "PlaybackWorker is already paused";
        return;
    }

    m_playing = false;
    // how long we were paused
    m_pauseStartMs = m_timer.elapsed();
    qDebug() << "Pausing at" << m_pauseStartMs << "ms";
}


void PlaybackWorker::resume() {
    qDebug() << "PlaybackWorker::resume called";
    QMutexLocker locker(&m_mutex);
    if (!m_running) {
        qWarning() << "Cannot resume, PlaybackWorker not running";
        return;
    }

    int64_t pauseEndMs = m_timer.elapsed();
    int64_t pauseDuration = pauseEndMs - m_pauseStartMs;
    qDebug() << "PlaybackWorker resume: pause duration =" << pauseDuration << "ms";
    m_nextWakeMs += pauseDuration;
    qDebug() << "Adjusted nextWakeMs to" << m_nextWakeMs;

     // reset timer for future elapsed()
    m_timer.restart(); 
    m_playing = true;
    m_cond.wakeOne();
    qDebug() << "PlaybackWorker resumed";
}


// TODO: This is not handled properly yet, just that it is not blocked by sleep
void PlaybackWorker::step(){
    qDebug() << "PlaybackWorker::step called";
    QMutexLocker locker(&m_mutex);
    m_singleStep = true;
    qDebug() << "PlaybackWorker single step set";
    // Wake the thread to process the single step
    m_cond.wakeOne();  
    qDebug() << "PlaybackWorker cond.wakeOne() called in step";
}

void PlaybackWorker::stop() {
    qDebug() << "PlaybackWorker::stop called";
    QMutexLocker locker(&m_mutex);
    m_running = false;
    qDebug() << "PlaybackWorker running set to false";
    // Ensure the loop wakes and exits
    m_cond.wakeOne();  
    qDebug() << "PlaybackWorker cond.wakeOne() called in stop";
}


void PlaybackWorker::runPlaybackLoop() {
    qDebug() << "PlaybackWorker::runPlaybackLoop entered";
    while (m_running) {
        QMutexLocker locker(&m_mutex);
        qDebug() << "Playback loop: playing=" << m_playing << " singleStep=" << m_singleStep;
        while (!m_playing && !m_singleStep) {
            m_cond.wait(&m_mutex);
        }

        // int64_t waitTime = std::max<int64_t>(0, m_nextWakeMs - m_timer.elapsed());
        int64_t waitTime = 40;
        locker.unlock();
        qDebug() << "PlaybackWorker sleeping for" << waitTime << "ms";

        QThread::msleep(waitTime);

        // Notify tick to main thread 
        emit tick();
        qDebug() << "PlaybackWorker tick emitted";

        locker.relock();
        if (m_singleStep) m_singleStep = false;
    }
    qDebug() << "PlaybackWorker runPlaybackLoop exiting";
}


void PlaybackWorker::scheduleNext(int64_t deltaMs) {
    qDebug() << "PlaybackWorker::scheduleNext called with deltaMs=" << deltaMs;
    QMutexLocker locker(&m_mutex);
    m_nextWakeMs = m_timer.elapsed() + deltaMs;
    qDebug() << "Next wake set to" << m_nextWakeMs;
    m_cond.wakeOne();
    qDebug() << "cond.wakeOne() called in scheduleNext";
}
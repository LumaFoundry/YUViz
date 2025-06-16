#include "playBackWorker.h"

void PlaybackWorker::start() {
    QMetaObject::invokeMethod(this, [this]() {
        m_running = true;
        m_playing = true;
        m_timer.start();
        m_nextWakeMs = 0;
        runPlaybackLoop();
    }, Qt::QueuedConnection);
}


void PlaybackWorker::pause() {
    QMutexLocker locker(&m_mutex);
    if (!m_playing) return;

    m_playing = false;
    // how long we were paused
    m_pauseStartMs = m_timer.elapsed();
}


void PlaybackWorker::resume() {
    QMutexLocker locker(&m_mutex);
    if (!m_running) return;

    int64_t pauseEndMs = m_timer.elapsed();
    int64_t pauseDuration = pauseEndMs - m_pauseStartMs;
    m_nextWakeMs += pauseDuration;

     // reset timer for future elapsed()
    m_timer.restart(); 
    m_playing = true;
    m_cond.wakeOne();
}


// TODO: This is not handled properly yet, just that it is not blocked by sleep
void PlaybackWorker::step(){
    QMutexLocker locker(&m_mutex);
    m_singleStep = true;
    // Wake the thread to process the single step
    m_cond.wakeOne();  
}

void PlaybackWorker::stop() {
    QMutexLocker locker(&m_mutex);
    m_running = false;
    // Ensure the loop wakes and exits
    m_cond.wakeOne();  
}


void PlaybackWorker::runPlaybackLoop() {
    while (m_running) {
        QMutexLocker locker(&m_mutex);
        while (!m_playing && !m_singleStep) {
            m_cond.wait(&m_mutex);
        }

        int64_t waitTime = std::max<int64_t>(0, m_nextWakeMs - m_timer.elapsed());
        locker.unlock();

        QThread::msleep(waitTime);

        // Notify tick to main thread 
        emit tick();

        locker.relock();
        if (m_singleStep) m_singleStep = false;
    }
}


void PlaybackWorker::scheduleNext(int64_t deltaMs) {
    QMutexLocker locker(&m_mutex);
    m_nextWakeMs = m_timer.elapsed() + deltaMs;
    m_cond.wakeOne();
}
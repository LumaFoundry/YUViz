#pragma once

#include <cstdint>
#include "frameData.h"
#include "frameMeta.h"
#include <QMutex>
#include <QWaitCondition>

class FrameQueue{

    public:
        // Takes in FrameMeta to initialize the queue
        FrameQueue(FrameMeta meta);
        ~FrameQueue();

        // Getter for metaData
        std::shared_ptr<FrameMeta> metaPtr() const {return m_metaPtr;};

        // Getter the current frame data pointer for renderer
        // Should be thread-safe - block until there is frame to read
        FrameData* getHeadFrame();

        // Get the next frame to be loaded for decoder
        // Should be thread-safe - block until there is space to write
        FrameData* getTailFrame();

        // Incrementing head and tail
        // Should be thread-safe
        void incrementHead();
        void incrementTail();

        const int getSize() const { return queueSize; }


    private:
        // Size of the queue
        const int queueSize = 50;

        // Points to current frame
        int64_t head = 0;

        // Points to future un-loaded frame
        int64_t tail = 0;

        // Thread safety
        QMutex m_mutex;
        QWaitCondition m_canRead;
        QWaitCondition m_canWrite;

        // Frame metadata
        std::shared_ptr<FrameMeta> m_metaPtr;

        // Shared buffer for the raw YUV data
        std::shared_ptr<std::vector<uint8_t>> m_bufferPtr;

        // Frame data queue 
        std::vector<FrameData> m_queue;

        // Indicators for whether queue is full / empty
        // Should not be called explicitly, use getHeadFrame() and getTailFrame() instead
        bool isFull() const;
        bool isEmpty() const; 

};
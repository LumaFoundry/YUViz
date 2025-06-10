#pragma once

#include <cstdint>
#include <frameData.h>
#include <frameMeta.h>

class FrameQueue{

    public:
        // Takes in FrameMeta to initialize the queue
        FrameQueue(FrameMeta* meta);
        ~FrameQueue();

        // Getter for metaData
        FrameMeta* metaDataPtr() const {return m_meta;};

        // Getter the current frame data pointer for renderer
        FrameData* getHead();

        // Get the next frame to be loaded for decoder
        FrameData* getTail();

        // Incrementing head and tail
        void incrementHead();
        void incrementTail();


    private:
        // Size of the queue
        const int queueSize = 50;

        // Points to current frame
        int64_t head = 0;

        // Points to future un-loaded frame
        int64_t tail = 0;

        // Frame metadata
        FrameMeta* m_meta;

        // Memory pool for frame data
        std::shared_ptr<FrameData> memoryPool;


};
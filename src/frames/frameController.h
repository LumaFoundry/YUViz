#pragma once

# include <FrameQueue.h>
#include <FrameMeta.h>
// One controller per FrameQueue 

// TODO: Frame read control
// TODO: Pass frameData ptr to renderer
// TODO: Manage frameQueue(s) - threading?
// TODO: Playback control
// TODO: Frame rate control

// Call decoder to update memory by passing frameData pointer to decoder
void updateQueue();

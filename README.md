## A videoplayer for YUV formats and based on Qt6 framework

## Module descriptions

### frameData.cpp/h
This module stores pointers of YUV data, and pts. 

### frameMeta.cpp/h
This module stores metadata shared by all frames. 

### frameQueue.cpp/h
This module manages a frameData queue, which stores pointers to frameData. 
It also allocate a memory pool for storing YUV data. 

### frameController.cpp/h
This module controls timing, ask yuvReader to update the frameData, and ask the renderer to render the next frame. 
It also handles playback control. 

### yuvReader.cpp/h
This module reads frame data in YUV from video file. 
It is suggested to use FFMpeg as decoder/reader.
It should support as many existing YUV format as possible (Most of them are supported by FFMpeg). 
It should allow command line arguments input to select YUV format for reading those non-header YUV files. 
It should read the raw YUV data of each frame. 
It should have a function to be called by frameController, with inputs of frameData and its data pointers, write next frame's YUV data to that frameData. 
It should have functions to move the reader to next or previous frame, or maybe with a selected pts's frame (FFMpeg seek()).

### videoRenderer.cpp/h
This module reads YUV data inside frameData and generates textures, renders in custom shaders. 
It should handle different choice of graphics API. 
It should have a function to be called by frameController, to render the next frame. 
It should have a way to give its rendered frame to gui and let gui display it. 

### gui/
This module draws frames on a window. 
It should handle different choice of graphics API. 
It should allow some playback control, which calls functions inside frameControl. 

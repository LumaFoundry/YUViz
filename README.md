# A YUV Inspection Tool based on Qt6

## Description
A lightweight video inspection tool for YUV format files.

## Requirements
- CMake 3.16+
- C++17 compatible compiler
- Qt 6.9.0 or newer
- FFmpeg libraries (libavcodec, libavutil, libswscale, libavformat)
- PkgConfig
- Ninja (recommended build system)


## How to build
1. Install Dependencies
    ### Windows (MSYS2)
    ```bash
   pacman -Syu
   pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
   pacman -S \
    mingw-w64-x86_64-qt6-base \
    mingw-w64-x86_64-qt6-declarative \
    mingw-w64-x86_64-qt6-shadertools
   pacman -S mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-pkgconf
    ```

    ### Mac (Homebrew)
    ```bash
    brew install cmake ninja pkg-config
    brew install qt@6 ffmpeg
    ```

    ### Linux (Ubuntu/Debian)
    ```bash
    sudo wget https://download.qt.io/official_releases/online_installers/qt-online-installer-linux-x64-online.run
    sudo chmod +x qt-online-installer-linux-x64-online.run 
    ./qt-online-installer-linux-x64-online.run install qt6.9.1-sdk  # Follow the steps, you will need a Qt account
    ```
    
    **Configure environment variables (choose one option):**
    
    **Option A: Set environment variables once**
    ```bash
    export PATH=$HOME/Qt/6.9.1/gcc_64/bin:$PATH
    export CMAKE_PREFIX_PATH=$HOME/Qt/6.9.1/gcc_64
    ```
    
    **Option B: Use Qt's built-in qt-cmake**
    ```bash
    # Use this command in step 2 if you choose Option B
    $HOME/Qt/6.9.1/gcc_64/bin/qt-cmake .. -G Ninja
    ```

2. Clone and build
    ```bash
    git clone https://github.com/LokiW-03/qt6-videoplayer.git
    cd qt6-videoplayer
    mkdir build && cd build
    cmake .. -G Ninja # Linux option B user should use the command mentioned
    ninja
    ```

## Running the application
```bash
./videoplayer <input_file> -r <width>x<height> -f <fps> -d
```
Flags:
- `-r`: resolution, needed for YUV files
- `-f`: framerate, default=25
- `-d`: debug flag

Note: `-d` flag could affect performance and behaviour

## Module descriptions

#### `frames/` - Frame Data Management
- **frameData.cpp/h**: Stores YUV data pointers and presentation timestamps (pts)
- **frameMeta.cpp/h**: Manages metadata shared across all frames
- **frameQueue.cpp/h**: Implements a frame buffer with memory pooling for efficient YUV data storage

#### `controllers/` - Application Flow Control
- **timer.cpp/h**: Provides precise timing mechanisms for frame presentation
- **videoController.cpp/h**: Coordinates overall video playback operations for multiple videos
- **frameController.cpp/h**: Manages frame decoding, timing, updates, and rendering coordination for each video

#### `decoder/` - Video Decoding
- **videoDecoder.cpp/h**: Handles FFmpeg integration for decoding various formats, supports seeking

#### `rendering/` - Video Display
- **videoRenderer.cpp/h**: Low-level rendering component that uses Qt RHI to upload YUV data to GPU textures and render frames with custom shaders
- **videoRenderNode.cpp/h**: Integrates with Qt's scene graph system to bridge between Qt's rendering pipeline and the custom VideoRenderer

#### `ui/` - User Interface
- **videoWindow.cpp/h**: QML-exposable component that handles video display, user interactions (zooming, panning, selection), and manages the rendering pipeline

#### `utils/` - Helper Utilities
- Common utilities and helper functions

#### `shaders/` - Rendering Shaders
- Shaders for video processing and display

#### `qml/` - Qt QML Interface
- QML components for the user interface

#### `main.cpp` - Application Entry Point
- Initializes application, processes command line arguments

# A YUV Inspection Tool based on Qt6

![Build](https://github.com/LokiW-03/qt6-videoplayer/workflows/CI%20-%20Build/badge.svg)
![CD](https://github.com/LokiW-03/qt6-videoplayer/actions/workflows/cd.yml/badge.svg)
[![Latest Artifacts](https://img.shields.io/badge/Download-Latest%20Build-blue?style=flat-square&logo=github)](https://github.com/LokiW-03/qt6-videoplayer/actions/workflows/cd.yml)

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
### General Usage
```bash
./videoplayer [file1] [file2] [options]
```
For standard files like `.mp4`, just provide the path. For `.yuv` files, see below.

### YUV File Usage
For .yuv files, append parameters to the filename using colons. Resolution is mandatory. Framerate and pixel format are optional and can be in any order.
- Format: `<path>:<resolution>[:<framerate>][:<pixel_format>]`
- Resolution: `<width>x<height>` (e.g., 1920x1080)
- Framerate: Defaults to 25 if omitted.
- Pixel Format: Defaults to 420P if omitted.

```bash
# Mandatory resolution only (uses defaults for framerate and format)
./videoplayer video.yuv:1920x1080

# All parameters specified, with debug flag
./videoplayer video.yuv:30:444P:1920x1080 -d
```

### Example with Mixed Files
```bash
./videoplayer video.mp4 video.yuv:1920x1080:30
```

Options:
- `--help`: print application information and instructions
- `-d`, `--debug`: Enable debug output (can affect performance).
- `-q <size>`: Set frame queue size (default: 50).
- `-s`, `--software`: Force software decoding (disables hardware acceleration).

## Troubleshooting
### macOS Rendering Issues

On macOS systems, the default Metal graphics backend used by Qt 6 may cause issues (e.g., a crash).

To fix this, you can force the application to use the OpenGL backend by setting an environment variable before running the application.

```bash
# Prepend the variable to the command for a one-time run
QSG_RHI_BACKEND=opengl ./videoplayer [arguments...]
```

Alternatively, you can export the variable first, which will apply it to your entire terminal session:

```bash
export QSG_RHI_BACKEND=opengl
./videoplayer [arguments...]
```

For a permanent fix to avoid typing this every time, you can add the setting to your shell's configuration file. Run the following command to add the line to your ~/.zshrc file:

```bash
echo 'export QSG_RHI_BACKEND=opengl' >> ~/.zshrc
```

For the change to take effect, you must either restart your terminal or run `source ~/.zshrc`.

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

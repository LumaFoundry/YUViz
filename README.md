# YUViz: A Video Inspection Tool <img align="right" src="https://raw.githubusercontent.com/LumaFoundry/YUViz/refs/heads/master/src/icons/icon.png" width=150>

![Build](https://github.com/LumaFoundry/YUViz/workflows/CI%20-%20Build/badge.svg)
![CD](https://github.com/LumaFoundry/YUViz/workflows/CD%20-%20Release/badge.svg)
[![Latest Artifacts](https://img.shields.io/badge/Download-Latest%20Build-blue?style=flat-square&logo=github)](https://github.com/LumaFoundry/YUViz/actions/workflows/cd.yml)

## Description
YUViz is a fast, lightweight video inspection and frame comparison tool built using Qt6 and FFmpeg. Features include:
* Cross-platform support
* Support for raw and encoded video formats
* Ability to change color space / color range
* Ability to zoom and view pixel values
* Frame-accurate side-by-side video playback
* Difference view with PSNR metrics
* ... and much more

## Requirements
- CMake 3.16+
- C++17 compatible compiler
- Qt 6.9.0 or newer
- FFmpeg libraries (libavcodec, libavutil, libswscale, libavformat)
- PkgConfig
- Ninja (recommended build system)


## How to build
1. Install Dependencies
    ### Windows 
    #### MSYS2
    ```bash
    pacman -Syu
    pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
    pacman -S \
    mingw-w64-x86_64-qt6-base \
    mingw-w64-x86_64-qt6-declarative \
    mingw-w64-x86_64-qt6-shadertools
    pacman -S mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-pkgconf
    ```

    #### WSL

    1. Prereqs 
        ```bash
        sudo apt update
        sudo apt install -y \
        build-essential cmake ninja-build pkg-config git \
        ffmpeg libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavfilter-dev \
        libxkbcommon-dev libxkbcommon-x11-dev \
        python3-pip p7zip-full
        ```

    2. Install Qt using aqtinstall (`apt` only supported versions)
        ```bash
        python3 -m pip install --user aqtinstall
        ~/.local/bin/aqt install-qt -O $HOME/Qt linux desktop 6.9.1 gcc_64 -m qtshadertools
        ```
    - Note: Using WSL will use Linux(Ubuntu) UI

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


2. Clone and build

    ```bash
    git clone https://github.com/LokiW-03/YUViz.git
    cd YUViz
    mkdir build && cd build
    ```

    ### Mac / Windows (MSYS2)
    ```bash
    cmake .. -G Ninja
    ninja
    ```

    ### Linux / Windows WSL

    **Configure environment variables (choose one option):**
    
    **Option A: Set environment variables once**
    ```bash
    export PATH=$HOME/Qt/6.9.1/gcc_64/bin:$PATH
    export CMAKE_PREFIX_PATH=$HOME/Qt/6.9.1/gcc_64
    cmake .. -G Ninja
    ninja
    ```
    
    **Option B: Use Qt's built-in qt-cmake**
    ```bash
    # Use this command in step 2 if you choose Option B
    $HOME/Qt/6.9.1/gcc_64/bin/qt-cmake .. -G Ninja
    ```

## Running the application
### General Usage
```bash
./YUViz [file1] [file2] [options]
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
./YUViz video.yuv:1920x1080

# All parameters specified, with debug flag
./YUViz video.yuv:30:444P:1920x1080 -d min
```

### Example with Mixed Files
```bash
./YUViz video.mp4 video.yuv:1920x1080:30
```

Options:
- `--help`: print application information and instructions
- `-d min`, `--debug`: Enable debug output (can affect performance).
    - debug levels:
    `min`, `max`, `xx`, `xx:yy:...`
    - replace `xx` and `yy` to any class alias (e.g. for FrameController, `fc`)
- `-q <size>`: Set frame queue size (default: 50).
- `-s`, `--software`: Force software decoding (disables hardware acceleration).

## Troubleshooting
### macOS Rendering Issues

On macOS systems, the default Metal graphics backend used by Qt 6 may cause issues (e.g., a crash).

To fix this, you can force the application to use the OpenGL backend by setting an environment variable before running the application.

```bash
# Prepend the variable to the command for a one-time run
QSG_RHI_BACKEND=opengl ./YUViz [arguments...]
```

Alternatively, you can export the variable first, which will apply it to your entire terminal session:

```bash
export QSG_RHI_BACKEND=opengl
./YUViz [arguments...]
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
- **compareController.cpp/h**: Manages comparison, PSNR updates of two video frames

#### `decoder/` - Video Decoding
- **videoDecoder.cpp/h**: Handles FFmpeg integration for decoding various formats, supports seeking

#### `rendering/` - Video Display
- **videoRenderer.cpp/h**, **diffRenderer.cpp/h**: Low-level rendering component that uses Qt RHI to upload YUV data to GPU textures and render frames with custom shaders
- **videoRenderNode.cpp/h**, **diffRenderNode.cpp/h**: Integrates with Qt's scene graph system to bridge between Qt's rendering pipeline and the custom VideoRenderer

#### `ui/` - User Interface
- **videoWindow.cpp/h**, **diffWindow.cpp/h**: QML-exposable component that handles video display, user interactions (zooming, panning, selection), and manages the rendering pipeline

#### `utils/` - Helper Utilities
- Common utilities and helper functions

#### `shaders/` - Rendering Shaders
- Shaders for video processing and display

#### `qml/` - Qt QML Interface
- QML components for the user interface

#### `main.cpp` - Application Entry Point
- Initializes application, processes command line arguments

### License
This project is licensed under the MIT License. See `LICENSE.txt` for details.

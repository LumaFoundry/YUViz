cmake_minimum_required(VERSION 3.16)
project(qt6_videoplayer LANGUAGES CXX)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 告诉 CMake 去哪里找 Qt6
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt/lib/cmake")

# 查找 Qt6 模块：Gui + Quick（包含 SceneGraph/QSGRenderNode）
find_package(Qt6 REQUIRED COMPONENTS
  Core
  Gui
  Quick
  GuiPrivate
)

# 查找 FFmpeg 库（如果需要）
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
  libavcodec
  libavutil
)

# Add FFmpeg library search paths
link_directories(${FFMPEG_LIBRARY_DIRS})

# 源文件
set(SRC
  src/main.cpp
  src/frames/frameMeta.cpp
  src/frames/frameQueue.cpp
  src/frames/frameData.cpp
)

# 可执行文件
add_executable(videoplayer ${SRC})

# 包含目录
target_include_directories(videoplayer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/frames
  ${FFMPEG_INCLUDE_DIRS}
)

# Link FFmpeg libraries from pkg-config
target_link_directories(videoplayer PRIVATE ${FFMPEG_LIBRARY_DIRS})

# 链接库
target_link_libraries(videoplayer PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
  Qt6::GuiPrivate
  ${FFMPEG_LIBRARIES}
)

# 在 macOS 上运行时需要指定 Framework 搜索路径
if(APPLE)
  set_target_properties(videoplayer PROPERTIES
    MACOSX_BUNDLE TRUE
    INSTALL_NAME_DIR "@rpath"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path/../Frameworks;/opt/homebrew/opt/qt/lib"
  )
endif()
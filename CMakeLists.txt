cmake_minimum_required(VERSION 3.16)
project(qt6_videoplayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt/lib/cmake")

find_package(Qt6 REQUIRED COMPONENTS
  Core
  Gui
  Quick
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
  libavcodec
  libavutil
)

link_directories(${FFMPEG_LIBRARY_DIRS})

set(SRC
  src/main.cpp
  src/frames/frameMeta.cpp
  src/frames/frameQueue.cpp
  src/frames/frameData.cpp
  src/rendering/videoRenderer.cpp
)

add_executable(videoplayer ${SRC})

target_include_directories(videoplayer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/frames
  ${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(videoplayer PRIVATE ${FFMPEG_LIBRARY_DIRS})

target_link_libraries(videoplayer PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
  Qt6::GuiPrivate
  ${FFMPEG_LIBRARIES}
)

if(APPLE)
  set_target_properties(videoplayer PROPERTIES
    MACOSX_BUNDLE TRUE
    INSTALL_NAME_DIR "@rpath"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path/../Frameworks;/opt/homebrew/opt/qt/lib"
  )
endif()
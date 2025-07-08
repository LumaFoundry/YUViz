cmake_minimum_required(VERSION 3.16)
project(qt6_videoplayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_BUILD_TYPE Debug)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt/lib/cmake")
endif()


find_package(Qt6 REQUIRED COMPONENTS
  Core
  Gui
  Quick
  Concurrent
  Widgets
  ShaderTools
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
  libavcodec
  libavutil
  libswscale
  libavformat
)

find_package(Vulkan QUIET)

if(Vulkan_FOUND)
    add_compile_definitions(VULKAN_INSTALLED)
endif()

link_directories(${FFMPEG_LIBRARY_DIRS})

set(SRC
  src/main.cpp
  src/frames/frameMeta.cpp
  src/frames/frameQueue.cpp
  src/frames/frameData.cpp
  src/controller/frameController.cpp
  src/controller/playBackWorker.cpp
  src/controller/videoController.cpp
  src/controller/timer.cpp
  src/rendering/videoRenderer.cpp
  src/rendering/videoRenderNode.cpp
  src/decoder/videoDecoder.cpp
  src/utils/errorReporter.cpp
  src/ui/videoWindow.cpp
)

set(QRC_SOURCES src/qml/qml.qrc)
add_executable(videoplayer ${SRC} ${QRC_SOURCES})

file(GLOB_RECURSE SHADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.frag"
)

qt6_add_shaders(videoplayer "videoplayer_shaders"
    PREFIX "/shaders"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders"
    FILES ${SHADER_FILES}
)

# enable_testing()
# add_subdirectory(test)

target_include_directories(videoplayer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/frames
  ${CMAKE_CURRENT_SOURCE_DIR}/src/controller
  ${CMAKE_CURRENT_SOURCE_DIR}/src/decoder
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
  ${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(videoplayer PRIVATE ${FFMPEG_LIBRARY_DIRS})

target_link_libraries(videoplayer PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
  Qt6::GuiPrivate
  Qt6::Widgets
  Qt6::Concurrent
  Qt6::ShaderTools
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
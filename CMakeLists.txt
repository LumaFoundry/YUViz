cmake_minimum_required(VERSION 3.16)
project(YUViz LANGUAGES CXX)

set(TARGET_NAME YUViz)
set(APP_DISPLAY_NAME "YUViz")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)

if (APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt/lib/cmake")
endif ()


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
        libavfilter
)

find_package(Vulkan QUIET)

if (Vulkan_FOUND)
    add_compile_definitions(VULKAN_INSTALLED)
endif ()

link_directories(${FFMPEG_LIBRARY_DIRS})

set(SRC
        src/main.cpp
        src/frames/frameMeta.cpp
        src/frames/frameQueue.cpp
        src/frames/frameData.cpp
        src/controller/frameController.cpp
        src/controller/videoController.cpp
        src/controller/timer.cpp
        src/controller/compareController.cpp
        src/rendering/videoRenderer.cpp
        src/rendering/videoRenderNode.cpp
        src/decoder/videoDecoder.cpp
        src/utils/errorReporter.cpp
        src/utils/sharedViewProperties.cpp
        src/utils/videoFormatUtils.cpp
        src/utils/debugManager.cpp
        src/ui/videoWindow.cpp
        src/ui/videoLoader.cpp
        src/qml/qml.qrc
        src/utils/compareHelper.cpp
        src/ui/diffWindow.cpp
        src/rendering/diffRenderer.cpp
        src/rendering/diffRenderNode.cpp
)

if (APPLE)
    list(APPEND SRC src/utils/aboutHelper.mm)
else ()
    list(APPEND SRC src/utils/aboutHelper_stub.cpp)
endif ()

set(QRC_SOURCES
        src/qml/qml.qrc
        src/icons/icons.qrc
)

# Platform-specific icon resources
set(PLATFORM_ICON_SOURCES)
if (WIN32)
    set(app_icon_resource_windows "src/icons/app_icon.rc")
    list(APPEND PLATFORM_ICON_SOURCES src/icons/app_icon.rc)
endif ()

if (APPLE)
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    set(app_icon_macos "src/icons/icon.icns")
    set_source_files_properties(src/icons/icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND PLATFORM_ICON_SOURCES src/icons/icon.icns)
endif ()

add_executable(${TARGET_NAME}
        ${SRC}
        ${QRC_SOURCES}
        ${PLATFORM_ICON_SOURCES}
)

file(GLOB_RECURSE SHADER_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.frag"
)

qt6_add_shaders(${TARGET_NAME} "shaders"
        PREFIX "/shaders"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders"
        FILES ${SHADER_FILES}
)

# enable_testing()
# add_subdirectory(test)

target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/frames
        ${CMAKE_CURRENT_SOURCE_DIR}/src/controller
        ${CMAKE_CURRENT_SOURCE_DIR}/src/decoder
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(${TARGET_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})

target_link_libraries(${TARGET_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::GuiPrivate
        Qt6::Widgets
        Qt6::Concurrent
        Qt6::ShaderTools
        ${FFMPEG_LIBRARIES}
)

if (APPLE)
    set_target_properties(${TARGET_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_ICON_FILE icon.icns
            INSTALL_NAME_DIR "@rpath"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "@executable_path/../Frameworks;/opt/homebrew/opt/qt/lib"
    )
endif ()

# Get git tag for version
execute_process(
        COMMAND git describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Pass version info as compiler definitions
target_compile_definitions(${TARGET_NAME} PRIVATE
        APP_VERSION="${GIT_VERSION}"
        APP_NAME="${APP_DISPLAY_NAME}"
)
cmake_minimum_required(VERSION 3.16)
project(qt6_videoplayer LANGUAGES CXX)

set(TARGET_NAME videoplayer)
set(APP_DISPLAY_NAME "Visual Inspection Tool")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)

# Enable code coverage (default: OFF, can be enabled via -DENABLE_COVERAGE=ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
    message(STATUS "Code coverage enabled")
endif()

# Enable testing (default: OFF, can be enabled via -DBUILD_TESTS=ON)
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    message(STATUS "Unit tests will be built")
else()
    message(STATUS "Unit tests will be skipped")
endif()

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
        src/ui/videoWindow.cpp
        src/ui/videoLoader.cpp
        src/qml/qml.qrc
        src/utils/compareHelper.cpp
        src/ui/diffWindow.cpp
        src/rendering/diffRenderer.cpp
        src/rendering/diffRenderNode.cpp
)

set(QRC_SOURCES src/qml/qml.qrc)
add_executable(${TARGET_NAME} ${SRC} ${QRC_SOURCES})

file(GLOB_RECURSE SHADER_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.frag"
)

qt6_add_shaders(${TARGET_NAME} "${TARGET_NAME}_shaders"
        PREFIX "/shaders"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders"
        FILES ${SHADER_FILES}
)

# Only enable testing if BUILD_TESTS is ON
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

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
cmake_minimum_required(VERSION 3.16)
project(qt6_videoplayer_test LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)

find_package(Qt6 REQUIRED COMPONENTS
  Core
  Gui
  Quick
  Concurrent
  Widgets
  Test
  ShaderTools
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
  libavcodec
  libavutil
  libswscale
  libavformat
)

link_directories(${FFMPEG_LIBRARY_DIRS})

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
FetchContent_MakeAvailable(googletest)

if(TARGET gtest)
    set_target_properties(gtest PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ""
    )
endif()
if(TARGET gmock)
    set_target_properties(gmock PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ""
    )
endif()


set(SRC_SOURCES
    ${CMAKE_SOURCE_DIR}/src/frames/frameMeta.cpp
    ${CMAKE_SOURCE_DIR}/src/frames/frameData.cpp
    ${CMAKE_SOURCE_DIR}/src/frames/frameQueue.cpp
    ${CMAKE_SOURCE_DIR}/src/controller/frameController.cpp
    ${CMAKE_SOURCE_DIR}/src/controller/videoController.cpp
    ${CMAKE_SOURCE_DIR}/src/decoder/videoDecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/rendering/videoRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/errorReporter.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/videoWindow.cpp

)

# Enable testing
enable_testing()

# Find Qt6 Test module
find_package(Qt6 REQUIRED COMPONENTS Test GuiPrivate)

# Find Google Test (optional, for more advanced testing)
find_package(GTest QUIET)

# Test source files
set(TEST_SOURCES
    test_main.cpp
    utils/errorReporterTest.cpp
    utils/appConfigTest.cpp
    utils/psnrResultTest.cpp
    utils/videoFileInfoTest.cpp
    controller/timerTest.cpp
    controller/videoControllerTest.cpp
    controller/compareControllerTest.cpp
    decoder/videoDecoderTest.cpp
    rendering/videoRendererTest.cpp
    rendering/diffRendererTest.cpp
    ui/videoWindowTest.cpp
    ui/videoLoaderTest.cpp
)

# Source files needed for tests
set(TEST_IMPL_SOURCES
    ../src/utils/errorReporter.cpp
    ../src/utils/sharedViewProperties.cpp
    ../src/utils/compareHelper.cpp
    ../src/controller/timer.cpp
    ../src/controller/compareController.cpp
    ../src/controller/videoController.cpp
    ../src/controller/frameController.cpp
    ../src/decoder/videoDecoder.cpp
    ../src/rendering/videoRenderer.cpp
    ../src/rendering/diffRenderer.cpp
    ../src/rendering/videoRenderNode.cpp
    ../src/rendering/diffRenderNode.cpp
    ../src/ui/videoWindow.cpp
    ../src/ui/videoLoader.cpp
    ../src/ui/diffWindow.cpp
    ../src/frames/frameMeta.cpp
    ../src/frames/frameQueue.cpp
    ../src/frames/frameData.cpp
)

# Create test executable
add_executable(unit_tests ${TEST_SOURCES} ${TEST_IMPL_SOURCES})

# Add shader resources
file(GLOB_RECURSE SHADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/shaders/*.frag"
)

qt6_add_shaders(unit_tests "videoplayer_shaders"
    PREFIX "/shaders"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../src/shaders"
    FILES ${SHADER_FILES}
)

# Include directories
target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/frames
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/controller
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/decoder
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/rendering
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FFMPEG_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(unit_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Quick
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Test
    ${FFMPEG_LIBRARIES}
)

# Link directories
target_link_directories(unit_tests PRIVATE ${FFMPEG_LIBRARY_DIRS})

# Add test
add_test(NAME UnitTests COMMAND unit_tests)

# Set test properties
set_tests_properties(UnitTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Enable CTest integration
include(CTest)
cmake_minimum_required(VERSION 3.16)
project(YUViz_test LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)

# Enable coverage flags for GCC/Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(COVERAGE_COMPILE_FLAGS "-O0 -g --coverage")
    set(COVERAGE_LINK_FLAGS "--coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILE_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_LINK_FLAGS}")
endif()

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
    ${CMAKE_SOURCE_DIR}/src/frames/frameData.cpp
    ${CMAKE_SOURCE_DIR}/src/frames/frameMeta.cpp
    ${CMAKE_SOURCE_DIR}/src/frames/frameQueue.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/debugManager.cpp
    ${CMAKE_SOURCE_DIR}/src/controller/compareController.cpp
    ${CMAKE_SOURCE_DIR}/src/controller/frameController.cpp
    ${CMAKE_SOURCE_DIR}/src/controller/videoController.cpp
    ${CMAKE_SOURCE_DIR}/src/controller/timer.cpp
    ${CMAKE_SOURCE_DIR}/src/decoder/VideoDecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/rendering/VideoRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/rendering/VideoRenderNode.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/videoWindow.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/diffWindow.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/sharedViewProperties.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ErrorReporter.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/compareHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/videoFormatUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/videoFileInfo.h
    ${CMAKE_SOURCE_DIR}/src/utils/y4mParser.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/appConfig.h
    ${CMAKE_SOURCE_DIR}/src/rendering/DiffRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/rendering/DiffRenderNode.cpp
)


set(TEST_SOURCES
    frames/test_framequeue.cpp
    utils/test_debugmanager.cpp
    controller/test_framecontroller.cpp
    frames/test_framedata.cpp
    frames/test_framemeta.cpp
    controller/test_comparecontroller.cpp
    controller/test_videocontroller.cpp
    ui/test_videowindow.cpp
    utils/test_comparehelper.cpp
    utils/test_sharedviewproperties.cpp
    utils/test_errorreporter.cpp
)

foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)

    add_executable(
        ${test_name}
        ${test_file}
        ${SRC_SOURCES}
        # ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_decoder.h
        # ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_renderer.h
    )

    # Add coverage flags to test targets
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${test_name} PRIVATE -O0 -g --coverage)
        target_link_options(${test_name} PRIVATE --coverage)
    endif()

    file(GLOB_RECURSE SHADER_FILES
        "${CMAKE_SOURCE_DIR}/src/shaders/*.vert"
        "${CMAKE_SOURCE_DIR}/src/shaders/*.frag"
    )

    qt6_add_shaders(${test_name} "shaders"
        PREFIX "/shaders"
        BASE "${CMAKE_SOURCE_DIR}/src/shaders"
        FILES ${SHADER_FILES}
    )

    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/mock
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${FFMPEG_INCLUDE_DIRS}
    )

    target_link_libraries(${test_name} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::GuiPrivate
        Qt6::Widgets
        Qt6::Concurrent
        Qt6::Test
        Qt6::ShaderTools
        GTest::gtest
        GTest::gmock
        ${FFMPEG_LIBRARIES}
    )

    set_target_properties(${test_name} PROPERTIES AUTOMOC ON)

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
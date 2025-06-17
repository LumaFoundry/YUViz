cmake_minimum_required(VERSION 3.16)
project(qt6_videoplayer_test LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Test)

set(FRAME_SOURCES
    ${CMAKE_SOURCE_DIR}/src/frames/frameMeta.cpp
    ${CMAKE_SOURCE_DIR}/src/frames/frameData.cpp
    ${CMAKE_SOURCE_DIR}/src/frames/frameQueue.cpp
)

set(TEST_SOURCES
    frames/test_framequeue.cpp
    # frames/test_framedata.cpp
)

foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    
    add_executable(${test_name} ${test_file} ${FRAME_SOURCES})

    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${test_name} PRIVATE Qt6::Test)

    set_target_properties(${test_name} PROPERTIES AUTOMOC ON)

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
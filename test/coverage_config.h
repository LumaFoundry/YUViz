#pragma once

// Test coverage configuration
// This file contains configuration for test coverage analysis

// Enable coverage analysis if supported
#ifdef __GNUC__
    #define COVERAGE_ENABLED
    #define COVERAGE_START() __gcov_flush()
    #define COVERAGE_STOP() __gcov_flush()
#else
    #define COVERAGE_START()
    #define COVERAGE_STOP()
#endif

// Test configuration constants
namespace TestConfig {
    const int DEFAULT_TIMEOUT_MS = 5000;
    const int SHORT_TIMEOUT_MS = 1000;
    const int LONG_TIMEOUT_MS = 10000;
    
    // Test data paths
    const char* TEST_DATA_DIR = "test_data";
    const char* SAMPLE_VIDEO_FILE = "sample_video.mp4";
    const char* SAMPLE_YUV_FILE = "sample_video.yuv";
    
    // Test video properties
    const int TEST_VIDEO_WIDTH = 1920;
    const int TEST_VIDEO_HEIGHT = 1080;
    const int TEST_VIDEO_FPS = 25;
    const int TEST_VIDEO_DURATION_MS = 10000; // 10 seconds
}


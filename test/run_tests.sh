#!/bin/bash

# Test runner script for Qt6 Video Player
# This script builds and runs all tests with coverage analysis

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== Qt6 Video Player Test Suite ==="
echo "Building and running tests..."
echo "Project root: $PROJECT_ROOT"

# Build the project with tests
echo "Building project..."
cd "$PROJECT_ROOT"
mkdir -p build
cd build

# Configure with coverage support and ninja generator
# Enable both tests and coverage for test runs
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON -G Ninja

# Build the project
echo "Compiling..."
ninja

# Run tests
echo "Running tests..."
ctest --output-on-failure --verbose

# Generate simple coverage report if available
if command -v gcovr &> /dev/null; then
    echo "Generating simple coverage report..."
    mkdir -p coverage_reports
    
    gcovr --root .. \
          --html \
          --exclude="test/.*" \
          --exclude="build/.*" \
          --exclude=".*_autogen/.*" \
          --exclude=".*/moc_.*" \
          --exclude=".*/qrc_.*" \
          --exclude=".*/ui_.*" \
          --exclude=".*/shaders/.*" \
          --exclude=".*/qml/.*" \
          --exclude=".*/main.cpp" \
          --exclude=".*/CMakeLists.txt" \
          --exclude=".*/Makefile" \
          --exclude=".*/build.ninja" \
          --exclude=".*/googletest.*" \
          --exclude=".*/googlemock.*" \
          --exclude=".*/gtest.*" \
          --exclude=".*/gmock.*" \
          --exclude=".*/test_.*" \
          --exclude=".*Test\\.cpp" \
          --exclude=".*Test\\.h" \
          --exclude=".*\\.h" \
          -o coverage.html
    
    echo "Coverage report generated: coverage.html"
    echo "Open the HTML file in your browser to view coverage details"
elif command -v lcov &> /dev/null; then
    echo "Generating coverage report with lcov..."
    mkdir -p coverage_reports
    lcov --capture --directory . --output-file coverage_reports/coverage.info
    lcov --remove coverage_reports/coverage.info "test/*" "build/*" "*_autogen/*" "*/moc_*" "*/qrc_*" "*/ui_*" "*/shaders/*" "*/qml/*" "*/main.cpp" --output-file coverage_reports/coverage_filtered.info
    genhtml coverage_reports/coverage_filtered.info --output-directory html_report
    echo "Coverage report generated: html_report/"
else
    echo "No coverage tool found. Install gcovr or lcov for coverage reports."
fi

echo "=== Test run completed ==="

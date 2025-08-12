# Qt6 Video Player Test Suite

This directory contains comprehensive unit tests for the Qt6 Video Player project.

## Test Structure

The test suite is organized by component:

- **utils/**: Tests for utility classes (ErrorReporter, SharedViewProperties, CompareHelper)
- **controller/**: Tests for controller classes (Timer, FrameController, VideoController, CompareController)
- **frames/**: Tests for frame-related classes (FrameData, FrameMeta, FrameQueue)
- **decoder/**: Tests for video decoder components
- **rendering/**: Tests for rendering components
- **ui/**: Tests for UI components

## Running Tests

### Quick Start
```bash
# Run all tests
./test/run_tests.sh
```

### Manual Build and Run
```bash
# Build the project
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
ninja

# Run tests
ctest --output-on-failure --verbose
```

### Individual Test Classes
```bash
# Run specific test class
cd build
./unit_tests ErrorReporterTest
./unit_tests SharedViewPropertiesTest
./unit_tests TimerTest
```

## Test Coverage

The test suite aims for high code coverage across all components:

- **ErrorReporter**: 100% coverage of singleton pattern, signal emission, and configuration
- **SharedViewProperties**: 100% coverage of property getters/setters, zoom/pan operations
- **Timer**: 100% coverage of play/pause/seek operations and signal emissions
- **FrameQueue**: 100% coverage of queue operations and thread safety
- **Other Components**: Comprehensive coverage of public APIs and edge cases

## Test Features

### Signal Testing
Tests use `QSignalSpy` to verify signal emissions:
```cpp
QSignalSpy spy(&object, &Class::signalName);
// Perform action
QCOMPARE(spy.count(), 1);
```

### Thread Safety Testing
Tests verify thread-safe operations:
```cpp
// Test concurrent access patterns
QThread::msleep(10);
object.operation();
```

### Edge Case Testing
Tests cover boundary conditions and error scenarios:
- Empty/invalid input
- Extreme values
- Null pointers
- Memory management

## Coverage Analysis

To generate coverage reports:

1. Install coverage tools:
   ```bash
   # macOS
   brew install gcovr lcov
   
   # Ubuntu/Debian
   sudo apt-get install gcovr lcov
   ```

2. Run tests with coverage:
   ```bash
   ./test/run_tests.sh
   ```

3. View coverage report:
   - HTML report: `build/coverage_report.html`
   - Or: `build/coverage_report/index.html`

## Adding New Tests

1. Create test files in appropriate directory:
   ```cpp
   // test/component/componentTest.h
   class ComponentTest : public QObject {
       Q_OBJECT
   private slots:
       void testFunction();
   };
   ```

2. Add to CMakeLists.txt:
   ```cmake
   set(TEST_SOURCES
       # ... existing tests
       component/componentTest.cpp
   )
   ```

3. Add to test_main.cpp:
   ```cpp
   #include "component/componentTest.h"
   // ...
   QTest::qExec(new ComponentTest(), argc, argv);
   ```

## Test Guidelines

- Use descriptive test names
- Test both success and failure cases
- Verify signal emissions
- Test thread safety where applicable
- Include edge case testing
- Use English comments and debug messages
- Follow Qt Test framework conventions

## Continuous Integration

Tests are designed to run in CI environments:
- Use offscreen rendering: `QT_QPA_PLATFORM=offscreen`
- No GUI dependencies for unit tests
- Fast execution (< 30 seconds total)
- Clear pass/fail reporting


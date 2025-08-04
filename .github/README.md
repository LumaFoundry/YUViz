# GitHub Actions CI Configuration

This directory contains GitHub Actions workflows for continuous integration.

## Workflows

### `ci.yml`
Main CI workflow that runs on the `try-ci` branch only.

## Platform Support

### Linux (Ubuntu)
- **Dependencies**: Build tools, CMake, Ninja, FFmpeg libraries
- **Qt**: Qt 6.9.1 via official installer action
- **Environment**: Uses Option A from README (environment variables)
- **Build**: CMake + Ninja

### Windows
- **Dependencies**: MSYS2 with MinGW-w64
- **Qt**: Qt6 packages from MSYS2
- **FFmpeg**: MSYS2 packages
- **Build**: CMake + Ninja

### macOS
- **Dependencies**: Homebrew
- **Qt**: Qt@6 via Homebrew
- **FFmpeg**: Homebrew packages
- **Build**: CMake + Ninja

## Trigger Conditions

The workflow only runs when:
- Push to `try-ci` branch
- Pull request to `try-ci` branch

## Build Process

1. **Checkout**: Clone the repository
2. **Install dependencies**: Platform-specific package installation
3. **Configure**: Run CMake with Ninja generator
4. **Build**: Compile with Ninja
5. **Verify**: Check binary integrity and dependencies

## Requirements Met

✅ **CMake 3.16+**: All platforms  
✅ **C++17**: Default compiler settings  
✅ **Qt 6.9.0+**: Qt 6.9.1 installed  
✅ **FFmpeg libraries**: All required libraries installed  
✅ **PkgConfig**: Available on all platforms  
✅ **Ninja**: Used as build system  

## Notes

- Linux uses Qt installer action (equivalent to README Option A)
- Windows uses MSYS2 packages (matches README exactly)
- macOS uses Homebrew (matches README exactly)
- All platforms follow the exact build commands from README 
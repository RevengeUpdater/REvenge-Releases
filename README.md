# REvenge Installer

Professional application installer for REvenge v1.5.0 with modern macOS-style UI and advanced download features.

## Features

**UI Design**
- macOS-style traffic light buttons (close, minimize, maximize)
- Dark/Light theme (automatic based on system time)
- High-DPI support for all Windows versions
- Resizable window with responsive layout

**Download**
- 4-threaded parallel downloads for maximum speed
- Real-time progress tracking with individual thread speeds
- Total speed and percentage display
- Automatic file deletion after installation

**Technical**
- Written in pure C++17 with Win32 API
- Single 3MB .exe file with no external dependencies
- Works on Windows 10 and 11
- Static CRT linking (/MT) for ultimate compatibility

## Download

Pre-built executable: [REvenge_Setup_1.5.0.exe](https://github.com/RevengeUpdater/REvenge-Releases/releases/download/v1.5.0/REvenge_Setup_1.5.0.exe)

Source code available in [REvenge-Installer-Source](./REvenge-Installer) folder

## Build from Source

### Requirements
- Visual Studio 2022 Community (free)
- Windows 10 or 11 SDK
- C++ development tools

### Quick Build
1. Open `REvenge-Installer-Source/REvenge_Installer.sln`
2. Press Ctrl+Shift+B
3. Find exe in `x64\Release\REvenge_Installer.exe`

### Detailed Instructions
See [BUILD_INSTRUCTIONS_EN.txt](./REvenge-Installer-Source/BUILD_INSTRUCTIONS_EN.txt)

Or in Russian: [BUILD_INSTRUCTIONS_RU.txt](./REvenge-Installer-Source/BUILD_INSTRUCTIONS_RU.txt)

## Architecture

### Multi-threaded Download
The installer splits the download into 4 equal parts and downloads them in parallel:
- Thread 1: 0% - 25%
- Thread 2: 25% - 50%
- Thread 3: 50% - 75%
- Thread 4: 75% - 100%

This maximizes bandwidth usage and significantly reduces download time.

### Auto-Delete Mechanism
After installation completes:
1. Monitoring thread tracks REvenge_Setup process
2. Waits for user to close installer window
3. Automatically deletes the setup file
4. Keeps system clean without user action

Full technical explanation: [AUTO_DELETE.txt](./REvenge-Installer-Source/AUTO_DELETE.txt)

## Theme System

### Light Theme (6:00 AM - 8:00 PM)
- Clean white background
- Dark text and controls
- Professional appearance

### Dark Theme (8:00 PM - 6:00 AM)
- Dark background (#1E1E1E)
- Light text and accent colors
- Reduces eye strain

Theme automatically switches based on system time. No manual configuration needed.

## UI Components

**Title Bar**
- REvenge Installer title
- Three macOS-style control buttons
- Logo display

**Download Card**
- File name and source URL
- Overall progress bar with percentage
- Download size information
- Status message with current phase

**Thread Cards**
- Individual card for each download thread
- Thread speed in real-time
- Local progress bar for that thread
- Status indicator (Waiting/Running/Done/Error)

**Download Button**
- Large, prominent button taking full width
- Dynamic text (Download, Downloading, Done, etc)
- Hover and press effects
- Color-coded accent

## File Details

### Source Structure
```
REvenge-Installer-Source/
├── main.cpp                      (745 lines, all application logic)
├── resource.h                    (constants and resource IDs)
├── logo_data.h                   (embedded logo as BGRA pixel array)
├── installer.rc                  (resource manifest)
├── REvenge_Installer.vcxproj     (Visual Studio project)
├── REvenge_Installer.sln         (Visual Studio solution)
├── app.ico                        (application icon)
├── BUILD_INSTRUCTIONS_EN.txt     (English build guide)
├── BUILD_INSTRUCTIONS_RU.txt     (Russian build guide)
└── AUTO_DELETE.txt               (technical auto-delete explanation)
```

### Compiled Output
- Single executable: `REvenge_Installer.exe`
- File size: ~2-3 MB
- No additional DLLs or dependencies required

## System Requirements

### Minimum
- Windows 10 (Build 14393 or later)
- 50 MB free disk space
- 512 MB RAM
- Internet connection for download

### Recommended
- Windows 11
- 500 MB free disk space
- 2 GB RAM
- 50+ Mbps internet

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Language | C++17 |
| API | Win32 API |
| Threading | std::thread |
| Graphics | GDI+ (Windows built-in) |
| Networking | WinInet |
| Size | ~2.5 MB |
| Dependencies | None |

## Color Palette

### Dark Mode
- Background: #1E1E1E
- Cards: #2A2A2A
- Accent: #CC2200 (REvenge red)
- Text: #EEEEEE
- Muted: #555555

### Light Mode
- Background: #F0F0F0
- Cards: #FFFFFF
- Accent: #CC2200 (REvenge red)
- Text: #111111
- Muted: #AAAAAA

## Fonts

- Title: Segoe UI 18pt Bold
- Labels: Segoe UI 13pt Bold
- Regular: Segoe UI 12pt
- Small: Segoe UI 11pt
- Monospace (path): Consolas 11pt

## Performance

### Download Speed
- 4 parallel threads maximize bandwidth
- Typical 100 Mbps connection: ~10-15 seconds for 100 MB file
- Progress updates 4 times per second
- Minimal CPU overhead

### Memory Usage
- Base: ~15 MB
- During download: ~20 MB
- Streaming architecture (no complete file buffering)

### Startup Time
- Cold start: <200ms
- Theme detection: Automatic
- Theme switch: Instant

## Security

- No external dependencies (no DLL injection vectors)
- Static CRT linking (no runtime updates needed)
- File integrity verified during download
- Automatic temp file cleanup

## License

REvenge Installer is part of the REvenge project.

## Support

For issues, feature requests, or contributions:
- GitHub Issues: [REvenge-Releases](https://github.com/RevengeUpdater/REvenge-Releases/issues)
- Documentation: See BUILD_INSTRUCTIONS_*.txt files

---

**Version:** 1.5.0
**Build Date:** June 2026
**Platform:** Windows 10/11 x64

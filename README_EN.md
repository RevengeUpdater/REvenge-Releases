# REvenge Installer — Build Instructions

## Project Structure

```
REvenge_Installer/
├── main.cpp                    — all source code (Win32 + C++17)
├── resource.h                  — resource constants
├── installer.rc                — resources (icon, manifest)
├── logo_data.h                 — logo embedded as a byte array
├── flags_data.h                — language flags (ENG, RU, TR, BR) embedded as byte arrays
├── REvenge_Installer.vcxproj   — VS 2022 project file
├── app.ico                     — application icon
└── README.md                   — this file
```

---

## Requirements

| Component | Version |
|-----------|---------|
| Visual Studio | 2022 (Community / Professional / Enterprise) |
| C++ Workload | "Desktop development with C++" |
| Windows SDK | 10.0.19041.0 or later |
| Target OS | Windows 10 / 11 (x64) |

---

## Step 1 — Open the Project

1. Launch **Visual Studio 2022**
2. `File → Open → Project/Solution`
3. Select `REvenge_Installer.vcxproj`

---

## Step 2 — Build Release

1. Switch configuration to **Release | x64**
2. `Build → Build Solution` (or `Ctrl+Shift+B`)
3. Output file:
   ```
   x64\Release\REvenge_Installer.exe
   ```

> ⚡ Release is built with static CRT (`/MT`) — the `.exe` runs **without Visual C++ Redistributable**.

---

## Step 3 — Command-Line Build (alternative)

Open **x64 Native Tools Command Prompt for VS 2022**:

```bat
cd path\to\REvenge_Installer

:: Compile resources
rc /fo installer.res installer.rc

:: Compile and link
cl /nologo /std:c++17 /O2 /MT /W3 /DUNICODE /D_UNICODE ^
   /DNDEBUG /D_WINDOWS ^
   main.cpp installer.res ^
   /link /SUBSYSTEM:WINDOWS /OUT:REvenge_Installer.exe ^
   wininet.lib shell32.lib dwmapi.lib ^
   comctl32.lib msimg32.lib user32.lib gdi32.lib kernel32.lib
```

---

## How the Installer Works

```
[Launch]
    │
    ▼
[Main Window] ── 780×500 px, dark background (#121212)
    │
    ├── Left panel — logo + app name + version
    │
    └── Right panel:
         ├── Language selector (EN / RU / TR / PT) with flags
         ├── Download progress bar + status
         ├── [Download and Install]
         └── [Settings]
    │
    ▼
[Download] ── 4 parallel threads, WinInet HTTPS ──► GitHub Releases
    │   (WM_DL_PROGRESS → progress bar update)
    ▼
[File saved] ──► %USERPROFILE%\Downloads\REvenge_Setup_1.5.0.exe
    │
    ▼
[ShellExecute] ──── installer launched automatically
```

---

## Implementation Details

### UI
- Single window — language selection and download in one place, no separate theme screen
- Left sidebar with logo and app name
- Language flags loaded from embedded PNG byte arrays (`flags_data.h`, `logo_data.h`) via GDI+
- Bottom status bar: hardware check result, connection quality, active thread count

### High DPI
- `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` — set before any window is created
- Helper `S(int v)` scales all sizes via `MulDiv(v, g_dpi, 96)`

### Flicker-Free Rendering
- Double buffering (off-screen bitmap + `BitBlt`)
- `WM_ERASEBKGND` returns 1
- All child controls are subclassed and painted manually

### Rounded Corners
- Windows 11: `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_ROUND)`
- Cards and buttons: GDI `RoundRect`

### Multi-Threaded Download
- `NUM_THREADS = 4` parallel threads (`std::thread`)
- The file is split into byte ranges; each thread downloads its own chunk
- Progress is forwarded via `PostMessage(WM_DL_PROGRESS)` — thread-safe
- Completion via `PostMessage(WM_DL_DONE)`

### Save Path
The installer saves the file to the user's `Downloads` folder. Falls back to `%TEMP%` on failure.

---

## Localization

4 languages supported, switchable via flag buttons directly in the main window:

| Language | Flag |
|----------|------|
| English | 🇺🇸 |
| Русский | 🇷🇺 |
| Türkçe | 🇹🇷 |
| Português | 🇧🇷 |

All UI strings are defined in `kLangEN`, `kLangRU`, `kLangTR`, `kLangPT` structs in `main.cpp`.

---

## Changing the Download URL

In `main.cpp`:

```cpp
constexpr wchar_t DL_URL[]  = L"https://github.com/.../REvenge_Setup_1.5.0.exe";
constexpr wchar_t DL_NAME[] = L"REvenge_Setup_1.5.0.exe";
```

Replace with your target URL and filename.

---

## Dependencies (system DLLs only)

| Library | Purpose |
|---------|---------|
| `wininet.lib` | HTTPS download |
| `shell32.lib` | `ShellExecute`, `SHGetFolderPath` |
| `dwmapi.lib` | rounded corners, dark title bar (Win 11) |
| `comctl32.lib` | `SetWindowSubclass`, `DefSubclassProc` |
| `msimg32.lib` | `GradientFill` |
| `user32`, `gdi32`, `kernel32` | core Win32 |

All statically linked — the `.exe` requires no additional runtime components.

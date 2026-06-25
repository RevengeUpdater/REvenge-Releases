# REvenge Installer - Auto-Delete Mechanism

## How It Works

### Overview
The installer automatically deletes the downloaded setup file (`REvenge_Setup_1.5.0.exe`) after the installation process completes and the setup window closes.

### Step-by-Step Process

#### 1. **Download & Launch Phase**
```
User clicks "Download & Install" button
    ↓
Multi-threaded download starts (4 concurrent threads)
    ↓
File saved to: C:\Users\[Username]\Downloads\REvenge_Setup_1.5.0.exe
    ↓
When download completes → ShellExecute opens the setup file
```

#### 2. **Monitoring Thread Creation**
When the setup file is successfully downloaded and launched:

```cpp
std::thread([hw,filePath=g_savePath](){
    WatchAndDeleteFile(filePath);      // Start watching
    PostMessage(hw, WM_CLOSE, 0, 0);   // Close installer after done
}).detach();
```

A **separate detached thread** is spawned that:
- Captures the file path (`g_savePath`)
- Captures the main window handle (`hw`)
- Runs independently of the main application

#### 3. **Process Monitoring**
The `WatchAndDeleteFile()` function performs the following:

```cpp
static void WatchAndDeleteFile(const std::wstring& filePath)
{
    // Step 1: Wait for setup process to start
    Sleep(2000);  // Give OS time to launch the process
    
    // Step 2: Find REvenge_Setup_1.5.0.exe by name
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    // Iterate through all running processes
    // Search for process named "REvenge_Setup_1.5.0.exe"
    // Get its Process ID (PID)
    
    // Step 3: Wait for process to close
    HANDLE hProc = OpenProcess(SYNCHRONIZE, FALSE, targetPid);
    WaitForSingleObject(hProc, INFINITE);  // Block until process exits
    CloseHandle(hProc);
    
    // Step 4: Delete the file
    Sleep(2000);  // Let OS release file locks
    DeleteFileW(filePath.c_str());  // Permanently delete
}
```

#### 4. **Timeline Visualization**

```
T=0s     ┌─ User clicks Download button
         │
T=0-30s  ├─ 4 threads download file in parallel
         │  ├─ Thread 1: bytes 0-25MB
         │  ├─ Thread 2: bytes 25-50MB
         │  ├─ Thread 3: bytes 50-75MB
         │  └─ Thread 4: bytes 75-100MB
         │
T=30s    ├─ Download complete
         ├─ ShellExecute → REvenge_Setup.exe launches
         └─ Monitoring thread STARTS (independent)
            │
T=32s    └─ Setup window opens (user sees installer)
         
[User performs installation...]

T=120s   ├─ User closes setup window
         │  REvenge_Setup.exe process terminates
         │
T=122s   ├─ Monitoring thread detects process exit
         ├─ Waits 2 seconds for file locks to release
         └─ Deletes REvenge_Setup_1.5.0.exe
            
T=123s   └─ File is gone from Downloads folder
            Main installer also closes
```

### Key Features

| Aspect | Implementation |
|--------|-----------------|
| **No Console Window** | Uses native Win32 API directly (no cmd.exe) |
| **No User Interaction** | Completely automatic and silent |
| **Reliable Timing** | Uses `WaitForSingleObject()` - waits until actual process exit |
| **Unicode Support** | Handles file paths with Cyrillic, spaces, special chars |
| **Independent Execution** | Detached thread survives app closure |
| **Process Detection** | Searches by exact executable name using `CreateToolhelp32Snapshot` |

### File Path Handling

```
Input:  C:\Users\Пользователь\Downloads\REvenge_Setup_1.5.0.exe
        ↓
        [Supports Cyrillic characters]
        ↓
        [Supports spaces in path]
        ↓
        [Supports UNC paths]
        ↓
Deletion: Guaranteed at exact same path
```

### Error Handling

1. **Process not found in 60 seconds**
   - Default behavior: Wait 60 seconds, then delete anyway
   - Prevents orphaned files if setup fails to launch

2. **File locked by antivirus**
   - Waits 2 additional seconds after process exit
   - Gives OS time to release file handles

3. **Path invalid or corrupted**
   - `DeleteFileW()` silently fails (no error shown to user)
   - Setup file wasn't downloaded anyway in this case

### Technical Stack

- **Language**: C++17
- **API**: Windows Win32 API (no external dependencies)
- **Threading**: `std::thread` with lambda capture
- **Process Management**: `CreateToolhelp32Snapshot`, `CreateProcess`, `WaitForSingleObject`
- **File Operations**: Native `DeleteFileW()` function

### Why This Approach?

✅ **Advantages:**
- Runs in background without UI
- No cmd.exe window spawned
- Truly waits for process exit (not time-based)
- Works with any file path encoding
- No external tools required

❌ **Alternatives (rejected):**
- `cmd.exe /c timeout && del` → Shows console window, time-based
- Registry cleanup → Overkill, requires permissions
- Batch file → Requires file creation, less reliable
- Scheduled task → Requires elevated privileges


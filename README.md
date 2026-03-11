# VINYLwerk

Professional audio restoration for REAPER.

This repository contains the REAPER-specific implementation and is hosted as `VINYLwerk-reaper`.

VINYLwerk combines a high-performance C++ backend with a smooth REAPER integration to provide fast, high-quality audio restoration directly in your timeline.

## ✨ Features
- **Click Removal**: Advanced detection and removal of impulsive noise.
- **AI Denoise**: Spectral subtraction and AI-powered noise reduction.
- **Vintage Filters**: Precision Rumble and Hum filters.
- **Non-Destructive**: Processes audio offline and imports the result as a new take.

## 🚀 Quick Install (Recommended)

### 1. Via ReaPack
1. Add this repository URL to ReaPack: `https://github.com/flarkflarkflark/VINYLwerk-reaper/raw/master/index.xml`
2. Search for **VINYLwerk** and install.
3. ReaPack will automatically download the correct backend for your Operating System.

---

### 2. Manual Installer (GitHub Release)
1. Go to [Releases](https://github.com/flarkflarkflark/VINYLwerk-reaper/releases).
2. Download the ZIP file for your OS (Windows, macOS, or Linux).
3. Extract the ZIP.
4. Run the installer:
   - **Windows**: Right-click `install.ps1` and select "Run with PowerShell".
   - **macOS/Linux**: Run `install.sh` in a terminal.
5. Restart REAPER.

## 🎹 Usage
1. Select an audio item in REAPER.
2. Open the Actions list (`?`).
3. Search for `VINYLwerk` and run it.
4. Adjust settings and click OK.

## 🛠️ Developers: Build from Source
If you want to build the backend yourself:
```bash
git clone --recursive https://github.com/flarkflarkflark/VINYLwerk-reaper.git
cd VINYLwerk-reaper
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## 📄 License
MIT License. Created by flarkAUDIO.

# VINYLwerk for REAPER

**VINYLwerk** is a specialized REAPER integration of the Audio Restoration Suite DSP engine. It combines a powerful native C++ backend with a seamless Lua ReaScript frontend to provide professional-grade vinyl and tape restoration directly inside your REAPER workflow.

## Architecture

VINYLwerk consists of two main components:
1. **VINYLwerk CLI (Backend)**: A high-performance, headless C++ console application built with JUCE. It handles the heavy lifting: AI Denoising (ONNX), Click Removal, and Spectral Processing.
2. **VINYLwerk ReaScript (Frontend)**: A Lua script running natively inside REAPER. It provides the user interface, manages item selection, handles audio rendering/replacement, and orchestrates the backend CLI.

## Features
- **Item-based Processing**: Select any audio item in REAPER and apply instant restoration.
- **Non-Destructive**: Processes audio and adds it as a new take to the existing item.
- **AI Denoise**: RNNoise-based AI denoiser optimized for quick offline processing.
- **Click & Pop Removal**: Advanced crossfade and spline interpolation click removal.
- **Native REAPER Feel**: Integrates directly into your custom actions and toolbars.

## Installation

### 1. Build the Backend
You must build the `vinylwerk_cli` executable for your operating system.
```bash
git clone --recursive https://github.com/flarkflarkflark/VINYLwerk.git
cd VINYLwerk
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### 2. Install the ReaScript
1. Open REAPER.
2. Go to **Actions > Show action list**.
3. Click **New action > Load ReaScript**.
4. Select `Scripts/VINYLwerk.lua`.
5. *Important*: Ensure the compiled `vinylwerk_cli` executable is placed in the same folder as the `.lua` script, or update the script to point to its location.

## Usage
1. Select an audio item containing vinyl clicks or broadband noise.
2. Run the **VINYLwerk** script from your Action List (or assign it to a toolbar button).
3. Adjust the restoration parameters in the popup dialog.
4. Click OK. The item will be processed offline and a new, restored take will appear in its place.

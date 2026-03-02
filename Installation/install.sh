#!/bin/bash
# VINYLwerk Installer for macOS and Linux

echo "Installing VINYLwerk for REAPER..."

# Detect REAPER resource path
if [[ "$OSTYPE" == "darwin"* ]]; then
    REAPER_PATH="$HOME/Library/Application Support/REAPER"
else
    REAPER_PATH="$HOME/.config/REAPER"
fi

if [ ! -d "$REAPER_PATH" ]; then
    echo "Error: REAPER resource directory not found at $REAPER_PATH"
    exit 1
fi

INSTALL_DIR="$REAPER_PATH/Scripts/flarkAUDIO/VINYLwerk"
mkdir -p "$INSTALL_DIR"

# Get script location
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Copy files
cp "$SCRIPT_DIR/../Scripts/VINYLwerk.lua" "$INSTALL_DIR/"
if [ -f "$SCRIPT_DIR/../build/vinylwerk_cli_artefacts/Release/vinylwerk_cli" ]; then
    cp "$SCRIPT_DIR/../build/vinylwerk_cli_artefacts/Release/vinylwerk_cli" "$INSTALL_DIR/"
elif [ -f "$SCRIPT_DIR/vinylwerk_cli" ]; then
    cp "$SCRIPT_DIR/vinylwerk_cli" "$INSTALL_DIR/"
fi

chmod +x "$INSTALL_DIR/vinylwerk_cli"

echo "Successfully installed to $INSTALL_DIR"
echo "Please restart REAPER and load the script from the Actions list."

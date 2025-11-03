#!/bin/bash
set -euo pipefail

# Windows installer repackaging script for custom ossia score applications

PLATFORM="$1"
shift
QML_ITEMS=("$@")

echo "Creating Windows package..."

cd "$WORK_DIR"

# Use local installer or download from GitHub
if [[ -n "$LOCAL_INSTALLER" ]]; then
    echo "Using local installer: $LOCAL_INSTALLER"

    # Validate it's an .exe file
    if [[ ! "$LOCAL_INSTALLER" =~ \.exe$ ]]; then
        echo "Error: Local installer must be an .exe file for Windows"
        exit 1
    fi

    cp "$LOCAL_INSTALLER" "score-installer.exe"
else
    # Download the official Windows installer
    if [[ "$RELEASE_TAG" == "continuous" ]]; then
        INSTALLER_NAME="ossia.score-master-win64.exe"
    else
        VERSION="${RELEASE_TAG#v}"
        INSTALLER_NAME="ossia.score-${VERSION}-win64.exe"
    fi
    INSTALLER_URL="https://github.com/ossia/score/releases/download/${RELEASE_TAG}/${INSTALLER_NAME}"

    echo "Downloading: $INSTALLER_URL"

    if ! curl -L -f -o "score-installer.exe" "$INSTALLER_URL"; then
        echo "Error: Failed to download installer from $INSTALLER_URL"
        exit 1
    fi
fi

# Extract the NSIS installer using 7z
echo "Extracting installer..."
if command -v 7z &> /dev/null; then
    7z x -o"score-extracted" "score-installer.exe" > /dev/null
elif command -v 7za &> /dev/null; then
    7za x -o"score-extracted" "score-installer.exe" > /dev/null
else
    echo "Error: 7z or 7za is required to extract the Windows installer"
    echo "Please install p7zip or p7zip-full package"
    exit 1
fi

# The extracted contents should be in score-extracted
if [[ ! -d "score-extracted" ]]; then
    echo "Error: Failed to extract installer"
    exit 1
fi

# Find the main installation directory (usually contains score.exe)
INSTALL_DIR="score-extracted"
if [[ ! -f "$INSTALL_DIR/score.exe" ]]; then
    # Try to find score.exe in subdirectories
    SCORE_EXE=$(find score-extracted -name "score.exe" -type f | head -n 1)
    if [[ -n "$SCORE_EXE" ]]; then
        INSTALL_DIR=$(dirname "$SCORE_EXE")
    else
        echo "Error: Could not find score.exe in extracted installer"
        exit 1
    fi
fi

cd "$INSTALL_DIR"

# Copy QML files
echo "Adding custom QML files..."
QML_DEST="qml"
mkdir -p "$QML_DEST"

for item in "${QML_ITEMS[@]}"; do
    # Convert to absolute path if relative
    if [[ "$item" = /* ]]; then
        item_path="$item"
    else
        item_path="$(cd "$(dirname "$item")" && pwd)/$(basename "$item")"
    fi

    if [[ -f "$item_path" ]]; then
        echo "  Copying file: $(basename "$item_path")"
        cp "$item_path" "$QML_DEST/"
    elif [[ -d "$item_path" ]]; then
        echo "  Copying directory: $(basename "$item_path")"
        cp -r "$item_path" "$QML_DEST/"
    fi
done

# Copy score file if provided
if [[ -n "$SCORE_FILE" ]]; then
    echo "Adding score file..."
    cp "$SCORE_FILE" .
fi

# Rename original executable
echo "Creating custom launcher..."
mv score.exe ossia-score.exe

# Create launchers based on whether score file is provided
if [[ -n "$SCORE_BASENAME" ]]; then
    # With autoplay
    cat > score.bat << 'BATCH_EOF'
@echo off
REM Custom launcher for APP_NAME_PLACEHOLDER
set "SCRIPT_DIR=%~dp0"
"%SCRIPT_DIR%ossia-score.exe" --ui "%SCRIPT_DIR%qml\MAIN_QML_PLACEHOLDER" --autoplay "%SCRIPT_DIR%SCORE_FILE_PLACEHOLDER" %*
BATCH_EOF
    sed -i "s/APP_NAME_PLACEHOLDER/${APP_NAME}/g" score.bat
    sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" score.bat
    sed -i "s/SCORE_FILE_PLACEHOLDER/${SCORE_BASENAME}/g" score.bat

    cat > "launch-${APP_NAME}.ps1" << 'PS_EOF'
# Custom launcher for APP_NAME_PLACEHOLDER
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$QmlPath = Join-Path $ScriptDir "qml\MAIN_QML_PLACEHOLDER"
$ScorePath = Join-Path $ScriptDir "SCORE_FILE_PLACEHOLDER"
$ExePath = Join-Path $ScriptDir "ossia-score.exe"

& $ExePath --ui $QmlPath --autoplay $ScorePath $args
PS_EOF
    sed -i "s/APP_NAME_PLACEHOLDER/${APP_NAME}/g" "launch-${APP_NAME}.ps1"
    sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "launch-${APP_NAME}.ps1"
    sed -i "s/SCORE_FILE_PLACEHOLDER/${SCORE_BASENAME}/g" "launch-${APP_NAME}.ps1"

    cat > "${APP_NAME}.bat" << 'BATCH_EOF'
@echo off
REM Launcher for APP_NAME_PLACEHOLDER
set "SCRIPT_DIR=%~dp0"
"%SCRIPT_DIR%ossia-score.exe" --ui "%SCRIPT_DIR%qml\MAIN_QML_PLACEHOLDER" --autoplay "%SCRIPT_DIR%SCORE_FILE_PLACEHOLDER" %*
BATCH_EOF
    sed -i "s/APP_NAME_PLACEHOLDER/${APP_NAME}/g" "${APP_NAME}.bat"
    sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "${APP_NAME}.bat"
    sed -i "s/SCORE_FILE_PLACEHOLDER/${SCORE_BASENAME}/g" "${APP_NAME}.bat"
else
    # Without autoplay
    cat > score.bat << 'BATCH_EOF'
@echo off
REM Custom launcher for APP_NAME_PLACEHOLDER
set "SCRIPT_DIR=%~dp0"
"%SCRIPT_DIR%ossia-score.exe" --ui "%SCRIPT_DIR%qml\MAIN_QML_PLACEHOLDER" %*
BATCH_EOF
    sed -i "s/APP_NAME_PLACEHOLDER/${APP_NAME}/g" score.bat
    sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" score.bat

    cat > "launch-${APP_NAME}.ps1" << 'PS_EOF'
# Custom launcher for APP_NAME_PLACEHOLDER
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$QmlPath = Join-Path $ScriptDir "qml\MAIN_QML_PLACEHOLDER"
$ExePath = Join-Path $ScriptDir "ossia-score.exe"

& $ExePath --ui $QmlPath $args
PS_EOF
    sed -i "s/APP_NAME_PLACEHOLDER/${APP_NAME}/g" "launch-${APP_NAME}.ps1"
    sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "launch-${APP_NAME}.ps1"

    cat > "${APP_NAME}.bat" << 'BATCH_EOF'
@echo off
REM Launcher for APP_NAME_PLACEHOLDER
set "SCRIPT_DIR=%~dp0"
"%SCRIPT_DIR%ossia-score.exe" --ui "%SCRIPT_DIR%qml\MAIN_QML_PLACEHOLDER" %*
BATCH_EOF
    sed -i "s/APP_NAME_PLACEHOLDER/${APP_NAME}/g" "${APP_NAME}.bat"
    sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "${APP_NAME}.bat"
fi

# Go back to work directory
cd "$WORK_DIR"

# Create a ZIP package
echo "Creating ZIP package..."
OUTPUT_ZIP="${APP_NAME}-windows.zip"

if command -v zip &> /dev/null; then
    (cd "$(dirname "$INSTALL_DIR")" && zip -r -q "$WORK_DIR/$OUTPUT_ZIP" "$(basename "$INSTALL_DIR")")
elif command -v 7z &> /dev/null; then
    7z a -tzip "$OUTPUT_ZIP" "$INSTALL_DIR" > /dev/null
elif command -v 7za &> /dev/null; then
    7za a -tzip "$OUTPUT_ZIP" "$INSTALL_DIR" > /dev/null
else
    echo "Error: zip or 7z is required to create the package"
    exit 1
fi

if [[ ! -f "$OUTPUT_ZIP" ]]; then
    echo "Error: Failed to create ZIP package"
    exit 1
fi

# Move to output directory
mv "$OUTPUT_ZIP" "$OUTPUT_DIR/"

echo "✓ Created: $OUTPUT_DIR/$OUTPUT_ZIP"

# Create installation instructions
cat > "$OUTPUT_DIR/${APP_NAME}-windows-README.txt" << EOF
${APP_NAME} - Windows Installation
==================================

Installation:
1. Extract the ZIP file to a location of your choice
2. Run "${APP_NAME}.bat" to launch the application

Alternative launcher:
- You can also use "launch-${APP_NAME}.ps1" (PowerShell script)
- Or run "score.bat" directly

This package contains:
- Main UI: ${MAIN_QML}
EOF

if [[ -n "$SCORE_BASENAME" ]]; then
    cat >> "$OUTPUT_DIR/${APP_NAME}-windows-README.txt" << EOF
- Score file: ${SCORE_BASENAME}

The application will automatically launch with the custom UI and autoplay the score file.
EOF
else
    cat >> "$OUTPUT_DIR/${APP_NAME}-windows-README.txt" << EOF

The application will automatically launch with the custom UI.
EOF
fi

cat >> "$OUTPUT_DIR/${APP_NAME}-windows-README.txt" << EOF

Requirements:
- Windows 10 or later (64-bit)
- No additional installation required - all dependencies are included

Troubleshooting:
- If you get a "Windows protected your PC" message, click "More info"
  then "Run anyway"
- Make sure to extract ALL files from the ZIP before running

EOF

echo "✓ Windows package created successfully"

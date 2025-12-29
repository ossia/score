#!/bin/bash
set -euo pipefail

# Windows installer repackaging script for custom ossia score applications

PLATFORM="$1"
shift
QML_ITEMS=("$@")

echo "Creating Windows package..."

cd "$WORK_DIR"

# Download rcedit for changing the icons

if ! curl -L -f -o "rcedit.exe" "https://github.com/electron/rcedit/releases/download/v2.0.0/rcedit-x64.exe"; then
    echo "Error: Failed to download rcedit"
    exit 1
fi

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

# Remove NSIS-specific directories that we don't need
echo "Cleaning up NSIS metadata..."
rm -rf score-extracted/'$PLUGINSDIR' score-extracted/'$_OUTDIR' score-extracted/'$TEMP' 2>/dev/null || true

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
        echo "  Copying directory contents: $(basename "$item_path")"
        # Copy contents of directory, not the directory itself
        cp -r "$item_path"/* "$QML_DEST/" 2>/dev/null || true
        # Also copy hidden files
        cp -r "$item_path"/.[!.]* "$QML_DEST/" 2>/dev/null || true
    fi
done

# Copy score file if provided
if [[ -n "$SCORE_FILE" ]]; then
    echo "Adding score file..."
    cp "$SCORE_FILE" .
fi

# Rename original executable
echo "Creating custom launcher..."
mv score.exe app-bin.exe

# Create native C launcher
echo "Creating native launcher executable..."

# Generate C source code
cp launcher/launcher.c launcher.c
cat > launcher-defines.h << EOF
#pragma once

#define MAIN_QML \"${MAIN_QML}\"
#define SCORE_FILE \"${SCORE_BASENAME}\"
#define HAS_AUTOPLAY $([[ -n "$AUTOPLAY" ]] && echo 1 || echo 0)
#define HAS_SCORE $([[ -n "${SCORE_BASENAME}" ]] && echo 1 || echo 0)

#define SCORE_CUSTOM_APP_ORGANIZATION_NAME "$SCORE_CUSTOM_APP_ORGANIZATION_NAME"
#define SCORE_CUSTOM_APP_ORGANIZATION_DOMAIN "$SCORE_CUSTOM_APP_ORGANIZATION_DOMAIN"
#define SCORE_CUSTOM_APP_APPLICATION_NAME "$SCORE_CUSTOM_APP_APPLICATION_NAME"
#define SCORE_CUSTOM_APP_APPLICATION_VERSION "$SCORE_CUSTOM_APP_APPLICATION_VERSION"

EOF

# Compile the launcher
# Try clang first, then fall back to CC (usually gcc or msvc cl)
COMPILER=""
if command -v clang &> /dev/null; then
    COMPILER="clang"
    echo "Using clang to compile launcher"
elif [[ -n "${CC:-}" ]] && command -v "$CC" &> /dev/null; then
    COMPILER="$CC"
    echo "Using $CC to compile launcher"
elif command -v gcc &> /dev/null; then
    COMPILER="gcc"
    echo "Using gcc to compile launcher"
else
    echo "Warning: No C compiler found (tried clang, \$CC, gcc)"
    echo "Falling back to batch script launcher"
    exit 1
fi

$COMPILER -O2 -s -mwindows -o "${APP_NAME}.exe" launcher.c
rm -f launcher.c launcher-defines.h

# Set icon and properties
./rcedit.exe "${APP_NAME}" --set-icon "${APP_ICON_ICO}" --set-file-version "${APP_VERSION}"
./rcedit.exe "app-bin.exe" --set-icon "${APP_ICON_ICO}" --set-file-version "${APP_VERSION}"

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
2. Run "${APP_NAME}.exe" to launch the application

Alternative launchers:
- "${APP_NAME}.exe" - Native Windows executable (recommended)
- "launch-${APP_NAME}.ps1" - PowerShell script
- "${APP_NAME}.bat" - Batch script (if .exe compilation failed)

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

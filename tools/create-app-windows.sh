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
mv score.exe ossia-score.exe

# Create native C launcher
echo "Creating native launcher executable..."

# Generate C source code
cat > launcher.c << 'C_EOF'
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Configuration - will be replaced by sed
#define MAIN_QML "MAIN_QML_PLACEHOLDER"
#define SCORE_FILE "SCORE_FILE_PLACEHOLDER"
#define HAS_SCORE SCORE_HAS_VALUE_PLACEHOLDER

int main(int argc, char *argv[]) {
    char exe_path[MAX_PATH];
    char exe_dir[MAX_PATH];
    char qml_path[MAX_PATH];
    char qml_import_path[MAX_PATH];
    char score_path[MAX_PATH];
    char command_line[32768]; // Windows max command line length

    // Get the directory where this executable is located
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);

    // Extract directory path
    char *last_slash = strrchr(exe_path, '\\');
    if (last_slash) {
        size_t dir_len = last_slash - exe_path;
        strncpy(exe_dir, exe_path, dir_len);
        exe_dir[dir_len] = '\0';
    } else {
        strcpy(exe_dir, ".");
    }

    // Build paths
    snprintf(qml_path, MAX_PATH, "%s\\qml\\%s", exe_dir, MAIN_QML);
    snprintf(qml_import_path, MAX_PATH, "%s\\qml", exe_dir);

    // Set QML2_IMPORT_PATH environment variable
    char qml_env_var[MAX_PATH + 20];
    snprintf(qml_env_var, sizeof(qml_env_var), "QML2_IMPORT_PATH=%s", qml_import_path);
    _putenv(qml_env_var);

    // Build command line
    snprintf(command_line, sizeof(command_line), "\"%s\\ossia-score.exe\" --ui \"%s\"",
             exe_dir, qml_path);

    // Add score file if present
    if (HAS_SCORE) {
        snprintf(score_path, MAX_PATH, "%s\\%s", exe_dir, SCORE_FILE);
        strncat(command_line, " --autoplay \"", sizeof(command_line) - strlen(command_line) - 1);
        strncat(command_line, score_path, sizeof(command_line) - strlen(command_line) - 1);
        strncat(command_line, "\"", sizeof(command_line) - strlen(command_line) - 1);
    }

    // Add any additional command line arguments
    for (int i = 1; i < argc; i++) {
        strncat(command_line, " \"", sizeof(command_line) - strlen(command_line) - 1);
        strncat(command_line, argv[i], sizeof(command_line) - strlen(command_line) - 1);
        strncat(command_line, "\"", sizeof(command_line) - strlen(command_line) - 1);
    }

    // Create process
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Launch the process
    if (!CreateProcessA(
        NULL,           // Application name (NULL = use command line)
        command_line,   // Command line
        NULL,           // Process security attributes
        NULL,           // Thread security attributes
        TRUE,           // Inherit handles
        0,              // Creation flags
        NULL,           // Environment
        exe_dir,        // Current directory
        &si,            // Startup info
        &pi             // Process info
    )) {
        MessageBoxA(NULL, "Failed to launch ossia score", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Wait for the process to complete
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Get exit code
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    // Close handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exit_code;
}
C_EOF

# Replace placeholders
sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" launcher.c
if [[ -n "$SCORE_BASENAME" ]]; then
    sed -i "s/SCORE_FILE_PLACEHOLDER/${SCORE_BASENAME}/g" launcher.c
    sed -i "s/SCORE_HAS_VALUE_PLACEHOLDER/1/g" launcher.c
else
    sed -i "s/SCORE_FILE_PLACEHOLDER//g" launcher.c
    sed -i "s/SCORE_HAS_VALUE_PLACEHOLDER/0/g" launcher.c
fi

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
    # Create fallback batch script
    if [[ -n "$SCORE_BASENAME" ]]; then
        cat > "${APP_NAME}.bat" << 'BATCH_EOF'
@echo off
set "SCRIPT_DIR=%~dp0"
"%SCRIPT_DIR%ossia-score.exe" --ui "%SCRIPT_DIR%qml\MAIN_QML_PLACEHOLDER" --autoplay "%SCRIPT_DIR%SCORE_FILE_PLACEHOLDER" %*
BATCH_EOF
        sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "${APP_NAME}.bat"
        sed -i "s/SCORE_FILE_PLACEHOLDER/${SCORE_BASENAME}/g" "${APP_NAME}.bat"
    else
        cat > "${APP_NAME}.bat" << 'BATCH_EOF'
@echo off
set "SCRIPT_DIR=%~dp0"
"%SCRIPT_DIR%ossia-score.exe" --ui "%SCRIPT_DIR%qml\MAIN_QML_PLACEHOLDER" %*
BATCH_EOF
        sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "${APP_NAME}.bat"
    fi
    COMPILER=""
fi

# Compile if we have a compiler
if [[ -n "$COMPILER" ]]; then
    if $COMPILER -O2 -s -mwindows -o "${APP_NAME}.exe" launcher.c 2>&1; then
        echo "✓ Native launcher compiled successfully: ${APP_NAME}.exe"
        rm -f launcher.c
    else
        echo "Warning: Compilation failed, creating batch script fallback"
        rm -f launcher.c "${APP_NAME}.exe"  # Clean up any partial artifacts
        # Create fallback batch script
        if [[ -n "$SCORE_BASENAME" ]]; then
            cat > "${APP_NAME}.bat" << 'BATCH_EOF'
@echo off
set "SCRIPT_DIR=%~dp0"
"%SCRIPT_DIR%ossia-score.exe" --ui "%SCRIPT_DIR%qml\MAIN_QML_PLACEHOLDER" --autoplay "%SCRIPT_DIR%SCORE_FILE_PLACEHOLDER" %*
BATCH_EOF
            sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "${APP_NAME}.bat"
            sed -i "s/SCORE_FILE_PLACEHOLDER/${SCORE_BASENAME}/g" "${APP_NAME}.bat"
        else
            cat > "${APP_NAME}.bat" << 'BATCH_EOF'
@echo off
set "SCRIPT_DIR=%~dp0"
"%SCRIPT_DIR%ossia-score.exe" --ui "%SCRIPT_DIR%qml\MAIN_QML_PLACEHOLDER" %*
BATCH_EOF
            sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "${APP_NAME}.bat"
        fi
    fi
else
    # No compiler, remove the launcher.c we created
    rm -f launcher.c
fi

# Also create PowerShell launcher for users who prefer it
if [[ -n "$SCORE_BASENAME" ]]; then
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
else
    cat > "launch-${APP_NAME}.ps1" << 'PS_EOF'
# Custom launcher for APP_NAME_PLACEHOLDER
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$QmlPath = Join-Path $ScriptDir "qml\MAIN_QML_PLACEHOLDER"
$ExePath = Join-Path $ScriptDir "ossia-score.exe"

& $ExePath --ui $QmlPath $args
PS_EOF
    sed -i "s/APP_NAME_PLACEHOLDER/${APP_NAME}/g" "launch-${APP_NAME}.ps1"
    sed -i "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "launch-${APP_NAME}.ps1"
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

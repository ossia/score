#!/bin/bash
set -euo pipefail

# Windows installer repackaging script for custom ossia score applications

PLATFORM="$1"
shift
QML_ITEMS=("$@")

# Matches the installer names ci/win32.deploy.sh publishes
case "$PLATFORM" in
    windows) ARCH="x86_64" ;;
    windows-arm64) ARCH="aarch64" ;;
    *) echo "Error: Invalid Windows platform: $PLATFORM"; exit 1 ;;
esac

echo "Creating Windows package for $ARCH..."

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
        INSTALLER_NAME="ossia.score-master-${ARCH}.exe"
    else
        VERSION="${RELEASE_TAG#v}"
        INSTALLER_NAME="ossia.score-${VERSION}-${ARCH}.exe"
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
rm -f score-extracted/'[NSIS].nsi' 2>/dev/null || true

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

# Two files in the installer payload exist only because the installer exists, and
# we ship a zip:
#  - Uninstall.exe is NSIS's uninstaller stub, a real entry in the installer.
#    Whether 7-Zip surfaces it depends on its version: 23 and 25 give up on the
#    script ("NSIS-3 Unicode BadCmd=13") and skip it, 26 reads it and extracts
#    it, so it turns up in packages built where 7-Zip is current and not in ones
#    built next to it. Nothing installed anything here, so it has nothing to undo.
#  - score.ico is installed by cmake/ScoreDeploymentWindows.cmake purely so the
#    NSIS script can point the Start menu and desktop shortcuts at it. A zip
#    creates no shortcuts, and the app's own icon is compiled into the launcher
#    and app-bin.exe below, so this is 48 kB of ossia score branding in a
#    differently-branded package.
echo "Dropping installer-only files..."
rm -f Uninstall.exe uninstall.exe score.ico

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

# Copy score file and directory content if provided
if [[ -n "$SCORE_FILE" ]]; then
    SCORE_DIR="$(dirname "$SCORE_FILE")"
    echo "Adding score file..."
    cp "$SCORE_FILE" .
    echo "  Copying directory contents: $(basename "$SCORE_DIR")"
    cp -r "$SCORE_DIR"/* ./ 2>/dev/null || true
fi

# Create qrc
if [[ -n "$APP_QRC" ]]; then
    rcc "$APP_QRC" -o "resources.rcc"
fi

# Rename original executable
echo "Creating custom launcher..."
mv score.exe app-bin.exe

# Create native C launcher
echo "Creating native launcher executable..."

# Generate C source code
echo " =========== "
cp "$SCORE_SOURCE_DIR/tools/launcher/launcher.cpp" launcher.cpp
cat > launcher-defines.h << EOF
#pragma once

#define MAIN_QML "${MAIN_QML}"
#define SCORE_FILE "${SCORE_BASENAME}"
#define HAS_AUTOPLAY $([[ -n "$AUTOPLAY" ]] && echo 1 || echo 0)
#define HAS_SCORE $([[ -n "${SCORE_BASENAME}" ]] && echo 1 || echo 0)

#define SCORE_CUSTOM_APP_ORGANIZATION_NAME "$APP_ORGANIZATION"
#define SCORE_CUSTOM_APP_ORGANIZATION_DOMAIN "$APP_DOMAIN"
#define SCORE_CUSTOM_APP_APPLICATION_NAME "$APP_NAME"
#define SCORE_CUSTOM_APP_APPLICATION_VERSION "$APP_VERSION"

EOF

if [[ -n "$APP_ENVIRONMENT" ]]; then
    echo "#undef SCORE_ENVIRONMENT" >> launcher-defines.h
    echo '#define SCORE_ENVIRONMENT R"___(' >> launcher-defines.h
    cat "$APP_ENVIRONMENT" >> launcher-defines.h
    echo ')___"' >> launcher-defines.h
else
    echo '#define SCORE_ENVIRONMENT ""' >> launcher-defines.h
fi

# Compile the launcher
# Try clang first, then fall back to CC (usually gcc or msvc cl)
COMPILER=""
if command -v 'clang++' &> /dev/null; then
    COMPILER="clang++"
    # -SUBSYSTEM:WINDOWS is an MSVC/lld-link spelling; a MinGW clang's lld rejects it.
    if clang++ -dumpmachine 2>/dev/null | grep -q -- "-gnu"; then
        CXXFLAGS="-O3 -std=c++20 -mwindows"
    else
        CXXFLAGS="-O3 -std=c++20 -Xlinker -SUBSYSTEM:WINDOWS"
    fi
    echo "Using clang++ to compile launcher"
elif [[ -n "${CXX:-}" ]] && command -v "$CXX" &> /dev/null; then
    COMPILER="$CXX"
    CXXFLAGS="-O3 -std=c++20 -mwindows"
    echo "Using $CXX to compile launcher"
elif command -v 'g++' &> /dev/null; then
    COMPILER="g++"
    CXXFLAGS="-O3 -mwindows"
    echo "Using g++ to compile launcher"
else
    echo "Warning: No C compiler found (tried clang, \$CC, gcc)"
    echo "Falling back to batch script launcher"
    exit 1
fi

$COMPILER $CXXFLAGS -o "${APP_NAME}.exe" launcher.cpp -luser32 -lshell32
rm -f launcher.cpp launcher-defines.h

# Set icon and properties
# NOTE: for icons to work on Windows:
# magick convert my-icon.png -define icon:auto-resize=16,24,32,48,64,72,96,128,256 app.ico
RCEDIT_FLAGS=(
  --set-icon "${APP_ICON_ICO}"
  --set-file-version "${APP_VERSION}"
  --set-product-version "${APP_VERSION}"
  --set-version-string "CompanyName" "${APP_ORGANIZATION}"
  --set-version-string "FileDescription" "${APP_DESCRIPTION}"
  --set-version-string "InternalName" "${APP_NAME}"
  --set-version-string "LegalCopyright" "${APP_COPYRIGHT}"
  --set-version-string "License" "GPLv3"
  --set-version-string "Homepage" "https://${APP_DOMAIN}"
  --set-version-string "ProductName" "${APP_NAME}"
  --set-version-string "ProductVersion" "${APP_VERSION}"
)

# create-app.sh has already checked --app-ico is a file, unless the caller set
# SCORE_ALLOW_DEFAULT_ICON=1. Skipping this used to be silent, and it is the one
# step that brands the executables: without it the launcher has no icon resource
# at all and app-bin.exe keeps the icon score was built with, so the app shows up
# as ossia score in the taskbar.
if [[ -f "${APP_ICON_ICO}" ]]; then
  "$WORK_DIR/rcedit.exe" "${APP_NAME}.exe" "${RCEDIT_FLAGS[@]}"
  "$WORK_DIR/rcedit.exe" "app-bin.exe" "${RCEDIT_FLAGS[@]}"
else
  echo "Warning: no --app-ico, ${APP_NAME}.exe and app-bin.exe keep ossia score's icon"
fi

# Go back to work directory
cd "$WORK_DIR"

# Create a ZIP package
echo "Creating ZIP package..."
OUTPUT_ZIP="${APP_NAME_SAFE}-${PLATFORM}.zip"

if command -v zip &> /dev/null; then
    (cd "$INSTALL_DIR" && zip -r -q "$WORK_DIR/$OUTPUT_ZIP" .)
elif command -v 7z &> /dev/null; then
    (cd "$INSTALL_DIR" && 7z a -tzip "$WORK_DIR/$OUTPUT_ZIP" * > /dev/null)
elif command -v 7za &> /dev/null; then
    (cd "$INSTALL_DIR" && 7za a -tzip "$WORK_DIR/$OUTPUT_ZIP" * > /dev/null)
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
echo "✓ Windows package created successfully"

#!/bin/bash
set -euo pipefail

# Linux AppImage repackaging script for custom ossia score applications

PLATFORM="$1"
shift
QML_ITEMS=("$@")

# Extract architecture
case "$PLATFORM" in
    linux-x86_64) export ARCH="x86_64" ;;
    linux-aarch64) export ARCH="aarch64" ;;
    *) echo "Error: Invalid Linux platform: $PLATFORM"; exit 1 ;;
esac

echo "Creating Linux AppImage for $ARCH..."

cd "$WORK_DIR"

# Use local installer or download from GitHub
if [[ -n "$LOCAL_INSTALLER" ]]; then
    echo "Using local AppImage: $LOCAL_INSTALLER"

    # Validate it's an AppImage
    if [[ ! "$LOCAL_INSTALLER" =~ \.AppImage$ ]]; then
        echo "Error: Local installer must be an .AppImage file for Linux"
        exit 1
    fi

    cp "$LOCAL_INSTALLER" "score-original.AppImage"
    chmod +x score-original.AppImage
else
    # Download the official AppImage
    # URL format differs between versioned releases and continuous builds
    if [[ "$RELEASE_TAG" == "continuous" ]]; then
        APPIMAGE_NAME="ossia.score-master-linux-${ARCH}.AppImage"
    else
        # Remove 'v' prefix if present for the version in the filename
        VERSION="${RELEASE_TAG#v}"
        APPIMAGE_NAME="ossia.score-${VERSION}-linux-${ARCH}.AppImage"
    fi
    APPIMAGE_URL="https://github.com/ossia/score/releases/download/${RELEASE_TAG}/${APPIMAGE_NAME}"

    echo "Downloading: $APPIMAGE_URL"

    if ! curl -L -f -o "score-original.AppImage" "$APPIMAGE_URL"; then
        echo "Error: Failed to download AppImage from $APPIMAGE_URL"
        exit 1
    fi

    chmod +x score-original.AppImage
fi

# Extract the AppImage
echo "Extracting AppImage..."
./score-original.AppImage --appimage-extract > /dev/null 2>&1

if [[ ! -d "squashfs-root" ]]; then
    echo "Error: Failed to extract AppImage"
    exit 1
fi

cd squashfs-root

# Copy QML files and directories
echo "Adding custom QML files..."
QML_DEST="usr/bin/qml"
mkdir -p "$QML_DEST"

for item in "${QML_ITEMS[@]}"; do
    if [[ -f "$item" ]]; then
        echo "  Copying file: $(basename "$item")"
        cp "$item" "$QML_DEST/"
    elif [[ -d "$item" ]]; then
        echo "  Copying directory contents: $(basename "$item")"
        # Copy contents of directory, not the directory itself
        cp -r "$item"/* "$QML_DEST/" 2>/dev/null || true
        # Also copy hidden files
        cp -r "$item"/.[!.]* "$QML_DEST/" 2>/dev/null || true
    fi
done

# Copy score file if provided
if [[ -n "$SCORE_FILE" ]]; then
    echo "Adding score file..."
    cp "$SCORE_FILE" "usr/bin/"
fi

# Create custom AppRun script
echo "Creating custom launcher..."
if [[ -n "$SCORE_BASENAME" ]]; then
    # With score
    cat > AppRun << APPRUN_EOF
#!/bin/sh

export SCORE_CUSTOM_APP_ORGANIZATION_NAME="$APP_ORGANIZATION"
export SCORE_CUSTOM_APP_ORGANIZATION_DOMAIN="$APP_DOMAIN"
export SCORE_CUSTOM_APP_APPLICATION_NAME="$APP_NAME"
export SCORE_CUSTOM_APP_APPLICATION_VERSION="$APP_VERSION"

export QML2_IMPORT_PATH="\${APPDIR}/usr/bin/qml/"
export LD_LIBRARY_PATH="\${APPIMAGE_LIBRARY_PATH}:\${APPDIR}/usr/lib:\${LD_LIBRARY_PATH}"
"\${APPDIR}/usr/bin/linuxcheck" "\${APPDIR}/usr/bin/app-bin"

# Launch with custom UI and score
exec "\${APPDIR}/usr/bin/app-bin" \
    ${AUTOPLAY} \
    --ui "\${APPDIR}/usr/bin/qml/${MAIN_QML}" \
    "\${APPDIR}/usr/bin/${SCORE_BASENAME}" \
    "\$@"
APPRUN_EOF
else
    # Without custom score
    cat > AppRun << APPRUN_EOF
#!/bin/sh

export SCORE_CUSTOM_APP_ORGANIZATION_NAME="$APP_ORGANIZATION"
export SCORE_CUSTOM_APP_ORGANIZATION_DOMAIN="$APP_DOMAIN"
export SCORE_CUSTOM_APP_APPLICATION_NAME="$APP_NAME"
export SCORE_CUSTOM_APP_APPLICATION_VERSION="$APP_VERSION"

export QML2_IMPORT_PATH="\${APPDIR}/usr/bin/qml/"
export LD_LIBRARY_PATH="\${APPIMAGE_LIBRARY_PATH}:\${APPDIR}/usr/lib:\${LD_LIBRARY_PATH}"
"\${APPDIR}/usr/bin/linuxcheck" "\${APPDIR}/usr/bin/app-bin"

# Launch with custom UI
exec "\${APPDIR}/usr/bin/app-bin" \
    ${AUTOPLAY} \
    --ui "\${APPDIR}/usr/bin/qml/${MAIN_QML}" \
    "$@"
APPRUN_EOF
fi

chmod +x AppRun

# Update desktop file
cat > "${APP_NAME_SAFE}.desktop" << DESKTOP_EOF
[Desktop Entry]
Type=Application
Name=${APP_NAME}
Comment=${APP_DESCRIPTION}
Exec=app-bin
Icon=${APP_NAME_SAFE}
Terminal=false
Categories=AudioVideo;
DESKTOP_EOF

# Update icons

cp "${APP_ICON_PNG}" "/tmp/build/score.AppDir/${APP_NAME_SAFE}.png"
cp "${APP_ICON_PNG}" "/tmp/build/score.AppDir/.DirIcon"

# Download appimagetool if needed
cd "$WORK_DIR"
if [[ ! -f "appimagetool-${ARCH}.AppImage" ]]; then
    echo "Downloading appimagetool..."
    curl -L -f -o "appimagetool-${ARCH}.AppImage" \
        "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH}.AppImage"
    chmod +x "appimagetool-${ARCH}.AppImage"
fi

# Download runtime if needed
if [[ ! -f "runtime-${ARCH}" ]]; then
    echo "Downloading AppImage runtime..."
    curl -L -f -o "runtime-${ARCH}" \
        "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-${ARCH}"
    chmod +x "runtime-${ARCH}"
fi

# Repackage the AppImage
echo "Repackaging AppImage..."
OUTPUT_APPIMAGE="${APP_NAME}-${PLATFORM}.AppImage"

./appimagetool-${ARCH}.AppImage -n squashfs-root "$OUTPUT_APPIMAGE" \
    --runtime-file "runtime-${ARCH}"

if [[ ! -f "$OUTPUT_APPIMAGE" ]]; then
    echo "Error: Failed to create AppImage"
    exit 1
fi

chmod +x "$OUTPUT_APPIMAGE"

# Move to output directory
mv "$OUTPUT_APPIMAGE" "$OUTPUT_DIR/"

echo "✓ Created: $OUTPUT_DIR/$OUTPUT_APPIMAGE"

# Create a simple run script
cat > "$OUTPUT_DIR/${APP_NAME}-${PLATFORM}.sh" << EOF
#!/bin/bash
# Launcher script for ${APP_NAME}
SCRIPT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
exec "\$SCRIPT_DIR/$OUTPUT_APPIMAGE" "\$@"
EOF

chmod +x "$OUTPUT_DIR/${APP_NAME}-${PLATFORM}.sh"

echo "✓ Linux package created successfully"

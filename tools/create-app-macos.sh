#!/bin/bash
set -euo pipefail

# macOS DMG repackaging script for custom ossia score applications

PLATFORM="$1"
shift
QML_ITEMS=("$@")

# Extract architecture
case "$PLATFORM" in
    macos-intel) ARCH="Intel" ;;
    macos-arm) ARCH="AppleSilicon" ;;
    *) echo "Error: Invalid macOS platform: $PLATFORM"; exit 1 ;;
esac

echo "Creating macOS package for $ARCH..."

cd "$WORK_DIR"

# Use local installer or download from GitHub
if [[ -n "$LOCAL_INSTALLER" ]]; then
    echo "Using local installer: $LOCAL_INSTALLER"

    if [[ "$LOCAL_INSTALLER" =~ \.app$ || "$LOCAL_INSTALLER" =~ \.app/ ]]; then
        # It's a .app bundle - copy directly
        echo "Copying .app bundle..."

        # Remove trailing slash if present
        LOCAL_APP="${LOCAL_INSTALLER%/}"

        if [[ ! -d "$LOCAL_APP" ]]; then
            echo "Error: .app bundle does not exist: $LOCAL_APP"
            exit 1
        fi

        cp -R "$LOCAL_APP" "${APP_NAME}.app"

    elif [[ "$LOCAL_INSTALLER" =~ \.dmg$ ]]; then
        # It's a DMG - mount and extract
        echo "Mounting local DMG..."

        # Capture the full hdiutil output to parse the mount point
        HDIUTIL_OUTPUT=$(hdiutil attach -nobrowse -readonly "$LOCAL_INSTALLER" 2>&1)

        # Extract mount point - find the line with /Volumes/ and extract everything after the last tab/spaces before /Volumes
        # The format is: /dev/diskXsY    TYPE    /Volumes/Name
        MOUNT_POINT=$(echo "$HDIUTIL_OUTPUT" | grep "/Volumes/" | tail -1 | sed -E 's/^.*(\/Volumes\/.*[^[:space:]]).*$/\1/' | sed 's/[[:space:]]*$//')

        if [[ -z "$MOUNT_POINT" ]] || [[ ! -d "$MOUNT_POINT" ]]; then
            echo "Error: Failed to mount DMG or locate mount point"
            echo "hdiutil output was:"
            echo "$HDIUTIL_OUTPUT"
            exit 1
        fi

        echo "Mounted at: $MOUNT_POINT"

        trap "hdiutil detach '$MOUNT_POINT' 2>/dev/null || true" EXIT

        # Find the .app bundle in the mounted DMG
        APP_IN_DMG=$(find "$MOUNT_POINT" -maxdepth 2 -name "*.app" | head -n 1)

        if [[ -z "$APP_IN_DMG" ]]; then
            echo "Error: No .app bundle found in DMG"
            hdiutil detach "$MOUNT_POINT"
            exit 1
        fi

        echo "Copying app bundle from DMG..."
        cp -R "$APP_IN_DMG" "${APP_NAME}.app"

        # Unmount the DMG
        hdiutil detach "$MOUNT_POINT"
        trap - EXIT

    else
        echo "Error: Local installer must be a .app bundle or .dmg file for macOS"
        exit 1
    fi
else
    # Download the official DMG
    # URL format differs between versioned releases and continuous builds
    if [[ "$RELEASE_TAG" == "continuous" ]]; then
        DMG_NAME="ossia.score-master-macOS-${ARCH}.dmg"
    else
        # Remove 'v' prefix if present for the version in the filename
        VERSION="${RELEASE_TAG#v}"
        DMG_NAME="ossia.score-${VERSION}-macOS-${ARCH}.dmg"
    fi
    DMG_URL="https://github.com/ossia/score/releases/download/${RELEASE_TAG}/${DMG_NAME}"

    echo "Downloading: $DMG_URL"

    if ! curl -L -f -o "score-original.dmg" "$DMG_URL"; then
        echo "Error: Failed to download DMG from $DMG_URL"
        exit 1
    fi

    # Mount the DMG
    echo "Mounting DMG..."
    # Capture the full hdiutil output to parse the mount point
    HDIUTIL_OUTPUT=$(hdiutil attach -nobrowse -readonly "score-original.dmg" 2>&1)

    # Extract mount point - find the line with /Volumes/ and extract everything after the last tab/spaces before /Volumes
    # The format is: /dev/diskXsY    TYPE    /Volumes/Name
    MOUNT_POINT=$(echo "$HDIUTIL_OUTPUT" | grep "/Volumes/" | tail -1 | sed -E 's/^.*(\/Volumes\/.*[^[:space:]]).*$/\1/' | sed 's/[[:space:]]*$//')

    if [[ -z "$MOUNT_POINT" ]] || [[ ! -d "$MOUNT_POINT" ]]; then
        echo "Error: Failed to mount DMG or locate mount point"
        echo "hdiutil output was:"
        echo "$HDIUTIL_OUTPUT"
        exit 1
    fi

    echo "Mounted at: $MOUNT_POINT"

    trap "hdiutil detach '$MOUNT_POINT' 2>/dev/null || true" EXIT

    # Find the .app bundle in the mounted DMG
    APP_IN_DMG=$(find "$MOUNT_POINT" -maxdepth 2 -name "*.app" | head -n 1)

    if [[ -z "$APP_IN_DMG" ]]; then
        echo "Error: No .app bundle found in DMG"
        hdiutil detach "$MOUNT_POINT"
        exit 1
    fi

    # Copy the app bundle
    echo "Copying app bundle..."
    cp -R "$APP_IN_DMG" "${APP_NAME}.app"

    # Unmount the original DMG
    hdiutil detach "$MOUNT_POINT"
    trap - EXIT
fi

# Modify the app bundle
APP_BUNDLE="${APP_NAME}.app"
BUNDLE_CONTENTS="$APP_BUNDLE/Contents"
BUNDLE_MACOS="$BUNDLE_CONTENTS/MacOS"

# Copy QML files
echo "Adding custom QML files..."
QML_DEST="$BUNDLE_MACOS/qml"
mkdir -p "$QML_DEST"

for item in "${QML_ITEMS[@]}"; do
    if [[ -f "$item" ]]; then
        echo "  Copying file: $(basename "$item")"
        cp "$item" "$QML_DEST/"
    elif [[ -d "$item" ]]; then
        echo "  Copying directory: $(basename "$item")"
        cp -r "$item" "$QML_DEST/"
    fi
done

# Copy score file if provided
if [[ -n "$SCORE_FILE" ]]; then
    echo "Adding score file..."
    cp "$SCORE_FILE" "$BUNDLE_MACOS/"
fi

# Rename the original binary
if [[ -f "$BUNDLE_MACOS/ossia score" ]]; then
    mv "$BUNDLE_MACOS/ossia score" "$BUNDLE_MACOS/ossia-score-bin"
elif [[ -f "$BUNDLE_MACOS/ossia-score" ]]; then
    mv "$BUNDLE_MACOS/ossia-score" "$BUNDLE_MACOS/ossia-score-bin"
else
    echo "Error: Could not find ossia score binary in app bundle"
    exit 1
fi

# Create launcher script
echo "Creating custom launcher..."
if [[ -n "$SCORE_BASENAME" ]]; then
    # With autoplay
    cat > "$BUNDLE_MACOS/ossia score" << 'LAUNCHER_EOF'
#!/bin/bash
# Custom launcher for APP_NAME_PLACEHOLDER
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export QML2_IMPORT_PATH="${SCRIPT_DIR}/qml/"
exec "$SCRIPT_DIR/ossia-score-bin" \
    --ui "$SCRIPT_DIR/qml/MAIN_QML_PLACEHOLDER" \
    --autoplay "$SCRIPT_DIR/SCORE_FILE_PLACEHOLDER" \
    "$@"
LAUNCHER_EOF
    # Replace placeholders
    sed -i '' "s/APP_NAME_PLACEHOLDER/${APP_NAME}/g" "$BUNDLE_MACOS/ossia score"
    sed -i '' "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "$BUNDLE_MACOS/ossia score"
    sed -i '' "s/SCORE_FILE_PLACEHOLDER/${SCORE_BASENAME}/g" "$BUNDLE_MACOS/ossia score"
else
    # Without autoplay
    cat > "$BUNDLE_MACOS/ossia score" << 'LAUNCHER_EOF'
#!/bin/bash
# Custom launcher for APP_NAME_PLACEHOLDER
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export QML2_IMPORT_PATH="${SCRIPT_DIR}/qml/"
exec "$SCRIPT_DIR/ossia-score-bin" \
    --ui "$SCRIPT_DIR/qml/MAIN_QML_PLACEHOLDER" \
    "$@"
LAUNCHER_EOF
    # Replace placeholders
    sed -i '' "s/APP_NAME_PLACEHOLDER/${APP_NAME}/g" "$BUNDLE_MACOS/ossia score"
    sed -i '' "s/MAIN_QML_PLACEHOLDER/${MAIN_QML}/g" "$BUNDLE_MACOS/ossia score"
fi

chmod +x "$BUNDLE_MACOS/ossia score"

# Update Info.plist
echo "Updating app metadata..."
if [[ -f "$BUNDLE_CONTENTS/Info.plist" ]]; then
    /usr/libexec/PlistBuddy -c "Set :CFBundleName ${APP_NAME}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName ${APP_NAME}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
fi

# Create a DMG (unsigned version - signing requires certificates)
echo "Creating DMG..."
OUTPUT_DMG="${APP_NAME}-${PLATFORM}.dmg"

# Simple method using hdiutil
rm -f "$OUTPUT_DMG"
hdiutil create -volname "$APP_NAME" -srcfolder "$APP_BUNDLE" -ov -format UDZO "$OUTPUT_DMG"

if [[ ! -f "$OUTPUT_DMG" ]]; then
    echo "Error: Failed to create DMG"
    exit 1
fi

# Move to output directory
mv "$OUTPUT_DMG" "$OUTPUT_DIR/"

echo "✓ Created: $OUTPUT_DIR/$OUTPUT_DMG"

# Create installation instructions
cat > "$OUTPUT_DIR/${APP_NAME}-${PLATFORM}-README.txt" << EOF
${APP_NAME} - macOS Installation
================================

1. Double-click the DMG file to mount it
2. Drag "${APP_NAME}.app" to your Applications folder
3. Launch from Applications or Launchpad

Note: This is an unsigned application. On first launch, you may need to:
- Right-click (or Control-click) the app and select "Open"
- Click "Open" in the security dialog

This package contains:
- Main UI: ${MAIN_QML}
EOF

if [[ -n "$SCORE_BASENAME" ]]; then
    cat >> "$OUTPUT_DIR/${APP_NAME}-${PLATFORM}-README.txt" << EOF
- Score file: ${SCORE_BASENAME}

The app will automatically launch with the custom UI and autoplay the score file.
EOF
else
    cat >> "$OUTPUT_DIR/${APP_NAME}-${PLATFORM}-README.txt" << EOF

The app will automatically launch with the custom UI.
EOF
fi

cat >> "$OUTPUT_DIR/${APP_NAME}-${PLATFORM}-README.txt" << EOF

EOF

echo "✓ macOS package created successfully"
echo "⚠ Note: Package is unsigned. Users will need to allow it in Security settings."

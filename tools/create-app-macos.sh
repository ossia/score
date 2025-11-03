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
BUNDLE_RESOURCES="$BUNDLE_CONTENTS/Resources"

# Create Resources directory
mkdir -p "$BUNDLE_RESOURCES"

# Copy QML files to Resources
echo "Adding custom QML files..."
QML_DEST="$BUNDLE_RESOURCES/qml"
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

# Copy score file if provided (to Resources, not MacOS)
if [[ -n "$SCORE_FILE" ]]; then
    echo "Adding score file..."
    cp "$SCORE_FILE" "$BUNDLE_RESOURCES/"
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
RESOURCES_DIR="$(cd "$SCRIPT_DIR/../Resources" && pwd)"
export QML2_IMPORT_PATH="${RESOURCES_DIR}/qml/"
exec "$SCRIPT_DIR/ossia-score-bin" \
    --ui "${RESOURCES_DIR}/qml/MAIN_QML_PLACEHOLDER" \
    --autoplay "${RESOURCES_DIR}/SCORE_FILE_PLACEHOLDER" \
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
RESOURCES_DIR="$(cd "$SCRIPT_DIR/../Resources" && pwd)"
export QML2_IMPORT_PATH="${RESOURCES_DIR}/qml/"
exec "$SCRIPT_DIR/ossia-score-bin" \
    --ui "${RESOURCES_DIR}/qml/MAIN_QML_PLACEHOLDER" \
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

# Code signing (optional - requires certificates)
if [[ -n "${MAC_CODESIGN_IDENTITY:-}" ]]; then
    echo "Code signing app bundle..."

    # Create entitlements file
    cat > "$WORK_DIR/entitlements.plist" << 'ENTITLEMENTS_EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
  <dict>
    <key>com.apple.security.cs.allow-jit</key>
    <true/>
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
    <key>com.apple.security.app-sandbox</key>
    <false/>
    <key>com.apple.security.cs.disable-library-validation</key>
    <true/>
    <key>com.apple.security.device.audio-input</key>
    <true/>
    <key>com.apple.security.device.camera</key>
    <true/>
    <key>com.apple.security.network.server</key>
    <true/>
    <key>com.apple.security.network.client</key>
    <true/>
    <key>com.apple.security.files.user-selected.read-write</key>
    <true/>
  </dict>
</plist>
ENTITLEMENTS_EOF

    # Remove QML and other resource files that shouldn't be signed
    echo "  Removing code signature from resource files..."
    find "$APP_BUNDLE" \( -name "*.qml" -o -name "*.qmlc" -o -name "*.js" -o -name "*.mjs" -o -name "*.txt" \) -type f -exec xattr -d com.apple.cs.CodeDirectory {} \; 2>/dev/null || true

    # Sign all dylibs first
    echo "  Signing dynamic libraries..."
    find "$APP_BUNDLE" -name '*.dylib' -type f -exec \
        codesign --force --timestamp --sign "$MAC_CODESIGN_IDENTITY" {} \; 2>/dev/null || true

    # Sign all frameworks
    echo "  Signing frameworks..."
    find "$APP_BUNDLE" -name "*.framework" -type d | while read framework; do
        codesign --force --timestamp --sign "$MAC_CODESIGN_IDENTITY" "$framework" 2>/dev/null || true
    done

    # Sign all executables (but not scripts or text files)
    echo "  Signing executables..."
    find "$BUNDLE_MACOS" -type f -perm +111 ! -name "*.qml" ! -name "*.qmlc" ! -name "*.js" ! -name "*.mjs" ! -name "*.txt" ! -name "*.sh" | while read executable; do
        # Check if it's actually a Mach-O binary
        if file "$executable" | grep -q "Mach-O"; then
            # Sign ossia-score-bin with entitlements and hardened runtime
            if [[ "$(basename "$executable")" == "ossia-score-bin" ]]; then
                echo "    Signing main binary with entitlements and hardened runtime..."
                codesign \
                    --entitlements "$WORK_DIR/entitlements.plist" \
                    --force \
                    --timestamp \
                    --options=runtime \
                    --sign "$MAC_CODESIGN_IDENTITY" \
                    "$executable" || {
                        echo "    Warning: Failed to sign $executable"
                    }
            else
                codesign --force --timestamp --sign "$MAC_CODESIGN_IDENTITY" "$executable" 2>/dev/null || true
            fi
        fi
    done

    # Sign nested app bundles (like vstpuppet, clappuppet, etc.)
    echo "  Signing nested bundles..."
    find "$BUNDLE_MACOS" -name "*.app" -type d -mindepth 1 | while read nested_app; do
        codesign --force --timestamp --options=runtime --sign "$MAC_CODESIGN_IDENTITY" "$nested_app" 2>/dev/null || true
    done

    # Sign the main app bundle with entitlements
    echo "  Signing main app bundle..."
    if codesign \
        --entitlements "$WORK_DIR/entitlements.plist" \
        --force \
        --timestamp \
        --options=runtime \
        --sign "$MAC_CODESIGN_IDENTITY" \
        "$APP_BUNDLE"; then
        echo "✓ App bundle signed successfully"

        # Verify signature (without --deep to avoid checking resource files)
        codesign --verify --strict "$APP_BUNDLE" && echo "✓ Signature verified"
    else
        echo "Warning: Code signing failed - app will be unsigned"
    fi
else
    echo "Skipping code signing (MAC_CODESIGN_IDENTITY not set)"
fi

# Create a DMG
echo "Creating DMG..."
OUTPUT_DMG="${APP_NAME}-${PLATFORM}.dmg"

# Simple method using hdiutil
rm -f "$OUTPUT_DMG"
hdiutil create -volname "$APP_NAME" -srcfolder "$APP_BUNDLE" -ov -format UDZO "$OUTPUT_DMG"

if [[ ! -f "$OUTPUT_DMG" ]]; then
    echo "Error: Failed to create DMG"
    exit 1
fi

# Sign the DMG if we have code signing credentials
if [[ -n "${MAC_CODESIGN_IDENTITY:-}" ]]; then
    echo "Signing DMG..."
    if codesign --force --timestamp --sign "$MAC_CODESIGN_IDENTITY" "$OUTPUT_DMG"; then
        echo "✓ DMG signed successfully"
    else
        echo "Warning: DMG signing failed"
    fi
fi

# Notarization (optional - requires Apple Developer credentials)
if [[ -n "${MAC_NOTARIZE_TEAM_ID:-}" ]] && [[ -n "${MAC_NOTARIZE_APPLE_ID:-}" ]] && [[ -n "${MAC_NOTARIZE_PASSWORD:-}" ]]; then
    echo "Notarizing DMG..."

    # Submit and capture the submission ID
    SUBMIT_OUTPUT=$(xcrun notarytool submit "$OUTPUT_DMG" \
        --team-id "$MAC_NOTARIZE_TEAM_ID" \
        --apple-id "$MAC_NOTARIZE_APPLE_ID" \
        --password "$MAC_NOTARIZE_PASSWORD" \
        --wait 2>&1)

    SUBMIT_STATUS=$?
    echo "$SUBMIT_OUTPUT"

    # Extract submission ID
    SUBMISSION_ID=$(echo "$SUBMIT_OUTPUT" | grep "id:" | head -1 | awk '{print $2}')

    if [[ $SUBMIT_STATUS -eq 0 ]] && echo "$SUBMIT_OUTPUT" | grep -q "status: Accepted"; then
        echo "✓ Notarization successful"

        # Staple the notarization ticket
        if xcrun stapler staple "$OUTPUT_DMG"; then
            echo "✓ Notarization ticket stapled"
            xcrun stapler validate "$OUTPUT_DMG" && echo "✓ Staple verified"
        else
            echo "Warning: Failed to staple notarization ticket"
        fi
    else
        echo "Warning: Notarization failed or returned invalid status"

        # Get detailed log if we have a submission ID
        if [[ -n "$SUBMISSION_ID" ]]; then
            echo "Fetching notarization log..."
            xcrun notarytool log "$SUBMISSION_ID" \
                --team-id "$MAC_NOTARIZE_TEAM_ID" \
                --apple-id "$MAC_NOTARIZE_APPLE_ID" \
                --password "$MAC_NOTARIZE_PASSWORD" 2>&1 || true
        fi

        echo "DMG will not be notarized"
    fi
else
    echo "Skipping notarization (credentials not set)"
fi

# Move to output directory
mv "$OUTPUT_DMG" "$OUTPUT_DIR/"

echo "✓ Created: $OUTPUT_DIR/$OUTPUT_DMG"

# Create installation instructions
SIGNED_NOTE=""
if [[ -z "${MAC_CODESIGN_IDENTITY:-}" ]]; then
    SIGNED_NOTE="Note: This is an unsigned application. On first launch, you may need to:
- Right-click (or Control-click) the app and select \"Open\"
- Click \"Open\" in the security dialog"
elif [[ -z "${MAC_NOTARIZE_TEAM_ID:-}" ]]; then
    SIGNED_NOTE="Note: This application is code-signed but not notarized. On first launch:
- Right-click (or Control-click) the app and select \"Open\"
- Click \"Open\" in the security dialog"
else
    SIGNED_NOTE="This application is code-signed and notarized by Apple."
fi

cat > "$OUTPUT_DIR/${APP_NAME}-${PLATFORM}-README.txt" << EOF
${APP_NAME} - macOS Installation
================================

1. Double-click the DMG file to mount it
2. Drag "${APP_NAME}.app" to your Applications folder
3. Launch from Applications or Launchpad

${SIGNED_NOTE}

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

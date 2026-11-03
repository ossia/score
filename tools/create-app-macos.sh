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

# Copy score file and directory content if provided (to Resources, not MacOS)
if [[ -n "$SCORE_FILE" ]]; then
    SCORE_DIR="$(dirname "$SCORE_FILE")"
    echo "Adding score file..."
    cp "$SCORE_FILE" "$BUNDLE_RESOURCES/"
    echo "  Copying directory contents: $(basename "$SCORE_DIR")"
    cp -r "$SCORE_DIR"/* "$BUNDLE_RESOURCES/" 2>/dev/null || true
fi

# macOS only shows privacy prompts (camera, microphone...) for the bundle's main
# executable: a launcher script in its place gets them refused without any prompt.
# So score itself is the main executable, and reads from Info.plist what a launcher
# would have passed it: LSEnvironment and ScoreArguments.
INFO_PLIST="$BUNDLE_CONTENTS/Info.plist"
if [[ "$(/usr/libexec/PlistBuddy -c "Print :ScoreCustomAppVersion" "$INFO_PLIST" 2>/dev/null)" != 1 ]]; then
    echo "Error: this ossia score cannot be packaged as a macOS custom app: it does"
    echo "       not read its settings from Info.plist (no ScoreCustomAppVersion = 1)."
    echo "       Use a more recent score release or build."
    exit 1
fi

# Find the original binary
SCORE_BINARY=""
for candidate in "ossia score" "ossia-score" "score"; do
    if [[ -f "$BUNDLE_MACOS/$candidate" ]]; then
        SCORE_BINARY="$BUNDLE_MACOS/$candidate"
        break
    fi
done
if [[ -z "$SCORE_BINARY" ]]; then
    echo "Error: Could not find ossia score binary in app bundle"
    exit 1
fi
MAIN_EXECUTABLE="$APP_NAME"
if [[ "$SCORE_BINARY" != "$BUNDLE_MACOS/$MAIN_EXECUTABLE" ]]; then
    mv "$SCORE_BINARY" "$BUNDLE_MACOS/$MAIN_EXECUTABLE"
fi

# Create rcc
if [[ -n "$APP_QRC" ]]; then
    rcc "$APP_QRC" -o "$BUNDLE_MACOS/resources.rcc"
fi

# Update Info.plist
echo "Updating app metadata..."
if [[ -f "$BUNDLE_CONTENTS/Info.plist" ]]; then
    /usr/libexec/PlistBuddy -c "Set :CFBundleName ${APP_NAME}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleExecutable ${APP_NAME}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName ${APP_NAME}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleGetInfoString ${APP_DESCRIPTION}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier ${APP_IDENTIFIER}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleLongVersionString ${APP_VERSION}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString ${APP_VERSION}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleVersion ${APP_VERSION}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Set :NSHumanReadableCopyright ${APP_COPYRIGHT}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Delete :CFBundleDocumentTypes" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Delete :UTExportedTypeDeclarations" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true

    # create-app.sh has already checked --app-icns is a file, unless the caller
    # set SCORE_ALLOW_DEFAULT_ICON=1. Without it CFBundleIconFile keeps pointing
    # at score.icns and the app shows score's icon in the Dock.
    if [[ -f "${APP_ICON_ICNS}" ]]; then
      ICNS_FILE_NAME="$(basename "${APP_ICON_ICNS}")"
      cp "${APP_ICON_ICNS}" "$BUNDLE_RESOURCES/${ICNS_FILE_NAME}"
      /usr/libexec/PlistBuddy -c "Set :CFBundleIconFile ${ICNS_FILE_NAME}" "$BUNDLE_CONTENTS/Info.plist" 2>/dev/null || true
      # The bundle we rebranded is ossia score's, so score.icns came with it. It
      # is nothing but score branding once CFBundleIconFile points elsewhere.
      if [[ "${ICNS_FILE_NAME}" != "score.icns" ]]; then
        rm -f "$BUNDLE_RESOURCES/score.icns"
      fi
    else
      echo "Warning: no --app-icns, the bundle keeps ossia score's icon"
    fi
    sed -i '' "s/ossia score/${APP_NAME}/g" "$BUNDLE_CONTENTS/Info.plist"
fi

# What score reads at startup. $RESOURCES_DIR is replaced by Contents/Resources.
echo "Writing the custom app settings..."
set_env() {
    plutil -replace "LSEnvironment.$1" -string "$2" "$INFO_PLIST"
}
plutil -replace LSEnvironment -xml '<dict/>' "$INFO_PLIST"
set_env SCORE_CUSTOM_APP_ORGANIZATION_NAME "$APP_ORGANIZATION"
set_env SCORE_CUSTOM_APP_ORGANIZATION_DOMAIN "$APP_DOMAIN"
set_env SCORE_CUSTOM_APP_APPLICATION_NAME "$APP_NAME"
set_env SCORE_CUSTOM_APP_APPLICATION_VERSION "$APP_VERSION"
set_env QML2_IMPORT_PATH '$RESOURCES_DIR/qml/'

# The environment file has the syntax the Windows launcher reads: KEY=VALUE lines,
# optionally after "export", values optionally quoted, # or ; starting comments.
# Nothing runs it through a shell, so whatever a shell would have expanded is
# refused rather than passed on literally.
if [[ -n "$APP_ENVIRONMENT" ]]; then
    line_number=0
    while IFS= read -r line || [[ -n "$line" ]]; do
        line_number=$((line_number + 1))
        line="${line%$'\r'}"
        line="${line#"${line%%[![:space:]]*}"}"
        line="${line%"${line##*[![:space:]]}"}"
        if [[ -z "$line" || "$line" == \#* || "$line" == \;* ]]; then
            continue
        fi
        if [[ "$line" =~ ^export[[:space:]]+(.*)$ ]]; then
            line="${BASH_REMATCH[1]}"
        fi
        if [[ ! "$line" =~ ^([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*=[[:space:]]*(.*)$ ]]; then
            echo "Error: $APP_ENVIRONMENT:$line_number: expected KEY=VALUE: $line"
            exit 1
        fi
        name="${BASH_REMATCH[1]}"
        value="${BASH_REMATCH[2]}"
        if [[ "$value" =~ ^\"(.*)\"$ || "$value" =~ ^\'(.*)\'$ ]]; then
            value="${BASH_REMATCH[1]}"
        fi
        rest="${value//\$\{RESOURCES_DIR\}/}"
        rest="${rest//\$RESOURCES_DIR/}"
        if [[ "$rest" == *'$'* || "$rest" == *'`'* ]]; then
            echo "Error: $APP_ENVIRONMENT:$line_number: only \$RESOURCES_DIR is expanded: $line"
            exit 1
        fi
        set_env "$name" "$value"
    done < "$APP_ENVIRONMENT"
fi

# --debug on the command line turns --ui into --ui-debug
SCORE_ARGUMENTS=()
if [[ -n "$AUTOPLAY" ]]; then
    SCORE_ARGUMENTS+=("$AUTOPLAY")
fi
SCORE_ARGUMENTS+=(--ui "\$RESOURCES_DIR/qml/${MAIN_QML}")
if [[ -n "$SCORE_BASENAME" ]]; then
    SCORE_ARGUMENTS+=("\$RESOURCES_DIR/${SCORE_BASENAME}")
fi
plutil -replace ScoreArguments -xml '<array/>' "$INFO_PLIST"
for i in "${!SCORE_ARGUMENTS[@]}"; do
    plutil -insert "ScoreArguments.$i" -string "${SCORE_ARGUMENTS[$i]}" "$INFO_PLIST"
done

# The app's own Info.plist entries. They replace score's, except that LSEnvironment
# is merged variable by variable. The keys that describe the score binary, or that
# the options above set, are refused, and so is removing a usage description:
# macOS kills an app that asks for a permission it has no description for.
if [[ -n "$APP_INFO_PLIST" ]]; then
    echo "Merging $APP_INFO_PLIST..."
    for key in CFBundleExecutable CFBundlePackageType CFBundleInfoDictionaryVersion \
               LSMinimumSystemVersion LSRequiresNativeExecution NSPrincipalClass \
               ScoreCustomAppVersion CFBundleName CFBundleDisplayName CFBundleIdentifier \
               CFBundleShortVersionString CFBundleVersion CFBundleLongVersionString \
               CFBundleIconFile; do
        if /usr/libexec/PlistBuddy -c "Print :$key" "$APP_INFO_PLIST" > /dev/null 2>&1; then
            echo "Error: $APP_INFO_PLIST must not set $key"
            exit 1
        fi
    done
    USAGE_KEYS=$(plutil -convert xml1 -o - "$INFO_PLIST" | sed -n 's/.*<key>\(NS[A-Za-z]*UsageDescription\)<\/key>.*/\1/p')

    # PlistBuddy's Merge keeps existing entries: merge score's into the app's.
    # Its commands split on spaces, which the bundle's path may contain.
    MERGED_PLIST="$WORK_DIR/Info.merged.plist"
    BASE_PLIST="$WORK_DIR/Info.base.plist"
    BASE_ENVIRONMENT="$WORK_DIR/Info.environment.plist"
    cp "$APP_INFO_PLIST" "$MERGED_PLIST"
    cp "$INFO_PLIST" "$BASE_PLIST"
    plutil -extract LSEnvironment xml1 -o "$BASE_ENVIRONMENT" "$BASE_PLIST"
    /usr/libexec/PlistBuddy -c "Merge $BASE_PLIST" "$MERGED_PLIST" > /dev/null
    /usr/libexec/PlistBuddy -c "Merge $BASE_ENVIRONMENT :LSEnvironment" "$MERGED_PLIST" > /dev/null
    plutil -convert xml1 -o "$INFO_PLIST" "$MERGED_PLIST"

    for key in $USAGE_KEYS; do
        if [[ -z "$(/usr/libexec/PlistBuddy -c "Print :$key" "$INFO_PLIST" 2>/dev/null)" ]]; then
            echo "Error: $APP_INFO_PLIST must not empty $key"
            exit 1
        fi
    done
fi
plutil -lint "$INFO_PLIST"

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
            # Sign score, the main executable, with entitlements and hardened runtime
            if [[ "$(basename "$executable")" == "$MAIN_EXECUTABLE" ]]; then
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
OUTPUT_DMG="${APP_NAME_SAFE}-${PLATFORM}.dmg"

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

echo "✓ macOS package created successfully"
echo "⚠ Note: Package is unsigned. Users will need to allow it in Security settings."

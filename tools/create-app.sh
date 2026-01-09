#!/bin/bash
set -euo pipefail

# Main orchestrator script for creating custom ossia score applications
# This script fetches official releases and repackages them with custom QML and score files

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCORE_SOURCE_DIR="$(realpath "$SCRIPT_DIR/..")"

# Default values
RELEASE_TAG="continuous"
QML_FILES=()
QML_DIRS=()
SCORE_FILE=""
OUTPUT_DIR=""
APP_NAME=""
APP_NAME_SAFE=""
APP_APPDATA_XML=""
APP_COPYRIGHT=""
APP_DESCRIPTION=""
APP_ENVIRONMENT=""
APP_ORGANIZATION=""
APP_DOMAIN=""
APP_IDENTIFIER=""
APP_ICON_ICO=""
APP_ICON_ICNS=""
APP_ICON_PNG=""
APP_VERSION=""
APP_QRC=""
AUTOPLAY=""
PLATFORMS=()
LOCAL_INSTALLER=""

# Parse arguments
show_help() {
    cat << EOF
Usage: $0 [OPTIONS]

Create custom ossia score applications from official releases.

Required options:
    --qml FILE/DIR      QML file or directory to include (can be specified multiple times)
                        The first .qml file will be used as the main UI
    --output DIR        Output directory for generated packages
    --name NAME         Name of the custom application
    --app-name NAME     Name of the custom application

Optional:
    --score FILE        Score file to use
    --autoplay          Auto-play upon load
    --local-installer PATH
                        Use a local installer instead of downloading from GitHub.
                        Path to: .AppImage (Linux), .app or .dmg (macOS), or .exe (Windows)
                        When specified, --release is ignored.
    --release TAG       Release tag to use (default: continuous)
                        Ignored if --local-installer is specified.
    --platform PLAT     Platform to build for: linux-x86_64, linux-aarch64,
                        macos-intel, macos-arm, windows
                        Can be specified multiple times. If not specified,
                        builds for current platform.
    --app-appdata-xml   Set the app appdata.xml file (Linux)
    --app-copyright     Set the app copyright (e.g. ossia.io)
    --app-description   Set the app description (one line max)
    --app-domain        Set the app domain (e.g. ossia.io)
    --app-environment   Set a file containing environment variables to set
    --app-icns          Set the app icon (icns format, macOS)
    --app-ico           Set the app icon (ico format, Windows)
    --app-identifier    Set the app identifier (e.g. io.ossia.score)
    --app-organization  Set the app organization (e.g. ossia)
    --app-png           Set the app icon (png format, Linux)
    --app-qrc           Optional qrc resource file
    --app-version       Set the app version (e.g. 1.0-rc3)
    --help              Show this help message

Example:
    $0 --qml App.qml --qml ./extra-qml --score App.score \\
       --output ./MyApp --name "My Custom App"

    Or without a score file:
    $0 --qml App.qml --output ./MyApp --name "My Custom App"

    Using a local installer:
    $0 --qml App.qml --score App.score --local-installer ./ossia.score.AppImage \\
       --output ./MyApp --name "My Custom App"

This will create platform-specific packages that launch score with:
    --ui App.qml [App.score] [--autoplay]

EOF
    exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --qml)
            if [[ -f "$2" ]]; then
                QML_FILES+=("$2")
            elif [[ -d "$2" ]]; then
                QML_DIRS+=("$2")
            else
                echo "Error: QML path does not exist: $2"
                exit 1
            fi
            shift 2
            ;;
        --score)
            SCORE_FILE="$2"
            if [[ ! -f "$SCORE_FILE" ]]; then
                echo "Error: Score file does not exist: $SCORE_FILE"
                exit 1
            fi
            # Convert to absolute path
            SCORE_FILE="$(cd "$(dirname "$SCORE_FILE")" && pwd)/$(basename "$SCORE_FILE")"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --name)
            APP_NAME="$2"
            shift 2
            ;;
        --app-name)
            APP_NAME="$2"
            shift 2
            ;;
        --app-environment)
            APP_ENVIRONMENT="$2"
            shift 2
            ;;
        --app-organization)
            APP_ORGANIZATION="$2"
            shift 2
            ;;
        --app-domain)
            APP_DOMAIN="$2"
            shift 2
            ;;
        --app-identifier)
            APP_IDENTIFIER="$2"
            shift 2
            ;;
        --app-description)
            APP_DESCRIPTION="$2"
            shift 2
            ;;
        --app-copyright)
            APP_COPYRIGHT="$2"
            shift 2
            ;;
        --app-version)
            APP_VERSION="$2"
            shift 2
            ;;
        --app-appdata-xml)
            APP_APPDATA_XML="$2"
            shift 2
            ;;
        --app-ico)
            APP_ICON_ICO="$2"
            shift 2
            ;;
        --app-icns)
            APP_ICON_ICNS="$2"
            shift 2
            ;;
        --app-png)
            APP_ICON_PNG="$2"
            shift 2
            ;;
        --app-qrc)
            APP_QRC="$2"
            shift 2
            ;;
        --autoplay)
            AUTOPLAY="--autoplay"
            shift 1
            ;;
        --release)
            RELEASE_TAG="$2"
            shift 2
            ;;
        --platform)
            PLATFORMS+=("$2")
            shift 2
            ;;
        --local-installer)
            LOCAL_INSTALLER="$2"
            if [[ ! -e "$LOCAL_INSTALLER" ]]; then
                echo "Error: Local installer does not exist: $LOCAL_INSTALLER"
                exit 1
            fi
            # Convert to absolute path
            LOCAL_INSTALLER="$(cd "$(dirname "$LOCAL_INSTALLER")" && pwd)/$(basename "$LOCAL_INSTALLER")"
            shift 2
            ;;
        --help)
            show_help
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            ;;
    esac
done

# Validate required arguments
if [[ ${#QML_FILES[@]} -eq 0 && ${#QML_DIRS[@]} -eq 0 ]]; then
    echo "Error: At least one --qml argument is required"
    show_help
fi

if [[ -z "$OUTPUT_DIR" ]]; then
    echo "Error: --output argument is required"
    show_help
fi

if [[ -z "$APP_NAME" ]]; then
    echo "Error: --name argument is required"
    show_help
fi

# Replace special characters with dashes in the name
APP_NAME_SAFE=$(echo "$APP_NAME" | sed -E 's/[^a-zA-Z0-9]+/-/g')

# Find the main QML file (first .qml file specified)
MAIN_QML=""
for qml in "${QML_FILES[@]+"${QML_FILES[@]}"}"; do
    if [[ "$qml" == *.qml ]]; then
        MAIN_QML="$(basename "$qml")"
        break
    fi
done

if [[ -z "$MAIN_QML" ]]; then
    # Try to find a .qml file in directories
    for dir in "${QML_DIRS[@]+"${QML_DIRS[@]}"}"; do
        found=$(find "$dir" -maxdepth 1 -name "*.qml" | head -n 1)
        if [[ -n "$found" ]]; then
            MAIN_QML="$(basename "$found")"
            break
        fi
    done
fi

if [[ -z "$MAIN_QML" ]]; then
    echo "Error: Could not find a .qml file to use as main UI"
    exit 1
fi

# Set score basename if score file is provided
if [[ -n "$SCORE_FILE" ]]; then
    SCORE_BASENAME="$(basename "$SCORE_FILE")"
else
    SCORE_BASENAME=""
fi

# Detect current platform if none specified
if [[ ${#PLATFORMS[@]} -eq 0 ]]; then
    case "$(uname -s)" in
        Linux)
            case "$(uname -m)" in
                x86_64) PLATFORMS+=("linux-x86_64") ;;
                aarch64|arm64) PLATFORMS+=("linux-aarch64") ;;
                *) echo "Error: Unsupported architecture: $(uname -m)"; exit 1 ;;
            esac
            ;;
        Darwin)
            case "$(uname -m)" in
                x86_64) PLATFORMS+=("macos-intel") ;;
                arm64) PLATFORMS+=("macos-arm") ;;
                *) echo "Error: Unsupported architecture: $(uname -m)"; exit 1 ;;
            esac
            ;;
        MINGW*|MSYS*|CYGWIN*)
            PLATFORMS+=("windows")
            ;;
        *)
            echo "Error: Unsupported platform: $(uname -s)"
            exit 1
            ;;
    esac
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"

echo "========================================="
echo "ossia score Custom App Builder"
echo "========================================="
echo "App Name:       $APP_NAME"
echo "Main QML:       $MAIN_QML"
if [[ -n "$SCORE_BASENAME" ]]; then
    echo "Score File:     $SCORE_BASENAME"
else
    echo "Score File:     (none)"
fi
if [[ -n "$LOCAL_INSTALLER" ]]; then
    echo "Source:         Local installer ($(basename "$LOCAL_INSTALLER"))"
else
    echo "Release:        $RELEASE_TAG"
fi
echo "Output Dir:     $OUTPUT_DIR"
echo "Platforms:      ${PLATFORMS[*]}"
echo "========================================="
echo

# Create a temporary working directory
WORK_DIR="$(mktemp -d)"
trap "rm -rf '$WORK_DIR'" EXIT

# Export variables for platform scripts
export APP_NAME
export APP_NAME_SAFE
export APP_APPDATA_XML
export APP_COPYRIGHT
export APP_DESCRIPTION
export APP_ORGANIZATION
export APP_DOMAIN
export APP_ENVIRONMENT
export APP_IDENTIFIER
export APP_ICON_ICO
export APP_ICON_ICNS
export APP_ICON_PNG
export APP_QRC
export APP_VERSION
export AUTOPLAY
export MAIN_QML
export SCORE_SOURCE_DIR
export SCORE_FILE
export SCORE_BASENAME
export OUTPUT_DIR
export RELEASE_TAG
export LOCAL_INSTALLER
export WORK_DIR
export SCRIPT_DIR
export SCORE_SOURCE_DIR

# Build for each platform
for platform in "${PLATFORMS[@]}"; do
    echo
    echo "Building for platform: $platform"
    echo "--------------------------------------"

    case "$platform" in
        linux-x86_64|linux-aarch64)
            "$SCRIPT_DIR/create-app-linux.sh" "$platform" "${QML_FILES[@]+"${QML_FILES[@]}"}" "${QML_DIRS[@]+"${QML_DIRS[@]}"}"
            ;;
        macos-intel|macos-arm)
            "$SCRIPT_DIR/create-app-macos.sh" "$platform" "${QML_FILES[@]+"${QML_FILES[@]}"}" "${QML_DIRS[@]+"${QML_DIRS[@]}"}"
            ;;
        windows)
            "$SCRIPT_DIR/create-app-windows.sh" "$platform" "${QML_FILES[@]+"${QML_FILES[@]}"}" "${QML_DIRS[@]+"${QML_DIRS[@]}"}"
            ;;
        *)
            echo "Error: Unknown platform: $platform"
            exit 1
            ;;
    esac

    echo "✓ Completed $platform"
done

echo
echo "========================================="
echo "All packages created successfully!"
echo "Output directory: $OUTPUT_DIR"
echo "========================================="

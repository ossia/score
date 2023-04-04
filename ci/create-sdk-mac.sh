#!/bin/bash -eux

: "$SDK_DIR"
: "$SCORE_DIR"

export OSSIA_SDK="/opt/ossia-sdk-x86_64"
export DST="$SDK_DIR"
export INCLUDE="$DST/usr/include"
export LIB="$DST/usr/lib"

mkdir -p "$INCLUDE"
mkdir -p "$LIB"
mkdir -p "$LIB/cmake/score"

export SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $SCRIPTDIR/create-sdk-common.sh

export XCODE=/Applications/Xcode.app/Contents/Developer
export XCODE_MACOS_SDK=$XCODE/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
export XCODE_MACOS_FRAMEWORKS=$XCODE_MACOS_SDK/System/Library/Frameworks
export XCODE_TOOLCHAIN=$XCODE/Toolchains/XcodeDefault.xctoolchain

# Copy OS API headers
rsync -ar "$XCODE_MACOS_SDK/usr/include/" "$INCLUDE/"
rsync -ar "$XCODE_TOOLCHAIN/usr/include/c++" "$INCLUDE/"

# Copy clang lib headers
export LLVM_VER=$(ls $OSSIA_SDK/llvm-libs/lib/clang | sort -r | head -1)
mkdir -p "$LIB/clang/$LLVM_VER"
rsync -ar "$OSSIA_SDK/llvm-libs/lib/clang/$LLVM_VER/include" "$LIB/clang/$LLVM_VER/"

# Copy frameworks
for fw in IOKit CFNetwork CoreFoundation CoreAudio CoreText Foundation DiskArbitration Accelerate AudioToolbox Security SystemConfiguration CoreGraphics ApplicationServices CoreServices Carbon Cocoa; do
  echo "$fw"
  mkdir -p "$INCLUDE/macos-sdks/$fw"
  rsync -ar $XCODE_MACOS_FRAMEWORKS/$fw.framework/Headers/ "$INCLUDE/macos-sdks/$fw/"

  if [[ -d $XCODE_MACOS_FRAMEWORKS/$fw.framework/Versions/A/Frameworks/ ]]; then
    for subfw_path in $XCODE_MACOS_FRAMEWORKS/$fw.framework/Versions/A/Frameworks/* ; do
      subfw=$(basename $subfw_path | cut -d'.' -f1)
      echo "  :: $subfw"
      mkdir -p "$INCLUDE/macos-sdks/$subfw"
      rsync -ar $subfw_path/Headers/ "$INCLUDE/macos-sdks/$subfw/"
    done
  fi
done

#!/bin/bash -eux

: "$SDK_DIR"
: "$SCORE_DIR"
: "$XCODE_ROOT"
: "$MACOS_ARCH"

export OSSIA_SDK="/opt/ossia-sdk-$MACOS_ARCH"
export DST="$SDK_DIR"
export INCLUDE="$DST/usr/include"
export LIB="$DST/usr/lib"

mkdir -p "$INCLUDE"
mkdir -p "$LIB"
mkdir -p "$LIB/cmake/score"

export SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source "$SCRIPTDIR/create-sdk-common.sh"

export XCODE=$XCODE_ROOT/Contents/Developer
export XCODE_MACOS_SDK=$XCODE/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
export XCODE_MACOS_FRAMEWORKS=$XCODE_MACOS_SDK/System/Library/Frameworks
export XCODE_TOOLCHAIN=$XCODE/Toolchains/XcodeDefault.xctoolchain

# Copy OS API headers
rsync -ar "$XCODE_MACOS_SDK/usr/include/" "$INCLUDE/"
rsync -ar "$XCODE_MACOS_SDK/usr/include/c++" "$INCLUDE/"

# Copy clang lib headers from the Xcode toolchain: macOS consumers build with
# AppleClang, whose builtins differ from the LLVM the ossia-sdk ships
export LLVM_VER=$(ls $XCODE_TOOLCHAIN/usr/lib/clang | sort -r | head -1)
mkdir -p "$LIB/clang/$LLVM_VER"
rsync -ar "$XCODE_TOOLCHAIN/usr/lib/clang/$LLVM_VER/include" "$LIB/clang/$LLVM_VER/"

# Ship the ORC runtime (orc_rt) so the JIT can use ExecutorNativePlatform. Unlike
# the headers above, orc_rt comes from the ossia-sdk LLVM (where it is built), not
# the Xcode toolchain; it lands under its own clang/<llvm-ver>/ dir.
ship_orc_runtime "$OSSIA_SDK/llvm-libs/lib/clang" "$LIB"

# Copy Qt frameworks
QT_FRAMEWORKS=$(find "$OSSIA_SDK/qt6-static/lib" -name '*.framework' | grep -E --only-matching 'Qt[a-zA-Z0-9_]+') 

for qt_framework in $QT_FRAMEWORKS; do
  if [[ -d "$OSSIA_SDK/qt6-static/lib/$qt_framework.framework/Versions/A/Headers" ]]; then
    cp -rf "$OSSIA_SDK/qt6-static/lib/$qt_framework.framework/Versions/A/Headers" "$INCLUDE/qt/$qt_framework"
  fi
done

# Copy frameworks
for fw in IOKit CFNetwork CoreFoundation CoreAudio CoreText Foundation DiskArbitration Accelerate AudioToolbox Security SystemConfiguration CoreGraphics ApplicationServices CoreServices Carbon Cocoa; do
  echo "$fw"
  mkdir -p "$INCLUDE/macos-sdks/$fw"
  rsync -ar "$XCODE_MACOS_FRAMEWORKS/$fw.framework/Headers/" "$INCLUDE/macos-sdks/$fw/" || true

  if [[ -d "$XCODE_MACOS_FRAMEWORKS/$fw.framework/Versions/A/Frameworks/" ]]; then
    for subfw_path in "$XCODE_MACOS_FRAMEWORKS/$fw.framework/Versions/A/Frameworks"/* ; do
      subfw=$(basename $subfw_path | cut -d'.' -f1)
      echo "  :: $subfw"
      mkdir -p "$INCLUDE/macos-sdks/$subfw"
      rsync -ar "$subfw_path/Headers/" "$INCLUDE/macos-sdks/$subfw/" || true
    done
  fi
done

# Make CoreFoundation's CF_ENUM / CF_CLOSED_ENUM compile as plain C++.
#
# Apple's CFAvailability.h, when a fixed underlying type is available -- which it
# is in C++ via cxx_strong_enums -- defines these macros with the Objective-C
# form `enum E : T E; enum E : T`, i.e. a non-defining fixed-underlying-type enum
# used as a declarator in the enclosing `typedef`. That is only legal in
# Objective-C(++) (objc_fixed_enum); the JIT compiles add-ons as C++23, where it
# is a hard error ("non-defining declaration of enumeration with a fixed
# underlying type is only permitted as a standalone declaration"), which clang's
# elaborated-enum-base diagnostic does not let us suppress in this LLVM build.
# Rewrite the C++ branch to the standards-compliant form newer SDKs already ship:
# a standalone forward declaration, a typedef of the (unfixed) enum, then the
# definition. The Objective-C branch is kept verbatim via #else.
CFAV="$INCLUDE/macos-sdks/CoreFoundation/CFAvailability.h"
if [[ -f "$CFAV" ]]; then
  perl -0pi -e 's{^(\#define (?:__CF_NAMED_ENUM|CF_CLOSED_ENUM)\(_type, _name\))[ \t]+enum (\S+) _name : _type _name; enum _name : _type[ \t]*$}{#if defined(__cplusplus)\n$1 enum $2 _name : _type; typedef enum _name _name; enum _name : _type\n#else\n$1 enum $2 _name : _type _name; enum _name : _type\n#endif}gm' "$CFAV"
  if grep -q '^#if defined(__cplusplus)$' "$CFAV" && grep -q 'typedef enum _name _name;' "$CFAV"; then
    echo "Patched CF_ENUM/CF_CLOSED_ENUM in CFAvailability.h for C++"
  else
    echo "WARNING: CFAvailability.h CF_ENUM patch did not apply -- macOS JIT may fail to compile CoreFoundation; check whether Apple changed the macro form"
  fi
fi

source $SCRIPTDIR/cleanup-sdk-common.sh

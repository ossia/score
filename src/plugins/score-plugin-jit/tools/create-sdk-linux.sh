#!/bin/bash

if [[ "x$SDK_DIR" == "x" ]]; then
  echo "SDK_DIR not set"
  exit 1
fi
export DST="$SDK_DIR" 
export INCLUDE="$DST/usr/include"
export LIB="$DST/usr/lib"
mkdir -p "$INCLUDE"
mkdir -p "$LIB"

(
    # Copy headers

    cd "$APP_DIR"
    for package in glibc-headers glibc-devel alsa-lib-devel mesa-libGL-devel libxkbcommon-x11-devel kernel-headers ; do
      (
        cd /usr/include
        rsync -aR $(rpm -ql $package | grep '/usr/include' | grep '\.h' | cut -c 14-  | sort | uniq) "$INCLUDE/"
      )
    done

    mkdir "$INCLUDE/qt"
    rsync -a "$OSSIA_SDK/qt5-static/include/" "$INCLUDE/qt/"

    rsync -a "$OSSIA_SDK/llvm/include/" "$INCLUDE/"
    rsync -a "$OSSIA_SDK/ffmpeg/include/" "$INCLUDE/"
    rsync -a "$OSSIA_SDK/fftw/include/" "$INCLUDE/"
    rsync -a "$OSSIA_SDK/portaudio/include/" "$INCLUDE/"

    export LLVM_VER=$(ls /opt/score-sdk/llvm/lib/clang/)
    mkdir -p "$LIB/clang/$LLVM_VER/include"
    rsync -a "$OSSIA_SDK/llvm/lib/clang/$LLVM_VER/include/" "$LIB/clang/$LLVM_VER/include/"
)

#!/bin/bash -x
BASE_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/..

source $BASE_DIR/tools/android.sourceme

cmake -DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-clang3.5 \
      -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/CMake/Android/qt-android-cmake/toolchain/android.toolchain.cmake \
      -DCMAKE_PREFIX_PATH=$QT_SDK/android_armv7/lib/cmake/Qt5 \
      "$BASE_DIR"
make
make score_apk

#adb uninstall net.score
#adb install base/app-android/bin/score-Android-debug-armeabi-v7a-03.apk
adb shell am start -n net.score/org.qtproject.qt5.android.bindings.QtActivity

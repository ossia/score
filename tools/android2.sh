#!/bin/bash -x
BASE_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/..

source android.sourceme

cmake -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/CMake/Android/qt-android-cmake/toolchain/android.toolchain.cmake -DCMAKE_PREFIX_PATH=$QT_SDK/android_armv7/lib/cmake/ ../i-score
make
make iscore_apk

#adb uninstall net.iscore
#adb install base/app-android/bin/iscore-Android-debug-armeabi-v7a-03.apk
adb shell am start -n net.iscore/org.qtproject.qt5.android.bindings.QtActivity

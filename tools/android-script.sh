#!/bin/bash -x
BASE_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/..

export ANDROID_NATIVE_API_LEVEL=19

export ANDROID_NDK=/opt/android-ndk
export ANDROID_SDK=/opt/android-sdk
export PATH=$ANDROID_SDK/tools:$ANDROID_SDK/platform-tools:$ANDROID_SDK/build-tools/android-4.4W:$PATH

cmake -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/CMake/Android/android.toolchain.cmake -DCMAKE_PREFIX_PATH=/opt/Qt/5.3/android_armv7/lib/cmake/ ../i-score
make -j8
make apk_debug

adb uninstall net.iscore
adb install base/app-android/bin/iscore-Android-debug-armeabi-v7a-0.3.apk    
adb shell am start -n net.iscore/org.qtproject.qt5.android.bindings.QtActivity

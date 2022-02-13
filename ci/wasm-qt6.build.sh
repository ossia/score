#!/bin/bash -eux

export SCORE_DIR=$PWD

mkdir -p /build || true
chown -R $(whoami) /build
cd /build

export QT=/opt/ossia-sdk-wasm/qt5

source /opt/ossia-sdk-wasm/emsdk/emsdk_env.sh
export CC=$(which emcc)
export CXX=$(which em++)

cmake -GNinja $SCORE_DIR \
   -DCMAKE_C_FLAGS="-O3 -g0" \
   -DCMAKE_CXX_FLAGS="-O3 -g0" \
   -DCMAKE_CXX_STANDARD=17 \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_UNITY_BUILD=1 \
   -DCMAKE_TOOLCHAIN_FILE=/opt/ossia-sdk-wasm/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
   -DOSSIA_PCH=0 \
   -DSCORE_PCH=0 \
   -DQt6_DIR=$QT/lib/cmake/Qt6 \
   -DQt6Core_DIR=$QT/lib/cmake/Qt6Core \
   -DQt6DeviceDiscoverySupport_DIR=$QT/lib/cmake/Qt6DeviceDiscoverySupport \
   -DQt6EdidSupport_DIR=$QT/lib/cmake/Qt6EdidSupport \
   -DQt6EglSupport_DIR=$QT/lib/cmake/Qt6EglSupport \
   -DQt6EventDispatcherSupport_DIR=$QT/lib/cmake/Qt6EventDispatcherSupport \
   -DQt6FbSupport_DIR=$QT/lib/cmake/Qt6FbSupport \
   -DQt6FontDatabaseSupport_DIR=$QT/lib/cmake/Qt6FontDatabaseSupport \
   -DQt6Gui_DIR=$QT/lib/cmake/Qt6Gui \
   -DQt6Multimedia_DIR=$QT/lib/cmake/Qt6Multimedia \
   -DQt6MultimediaQuick_DIR=$QT/lib/cmake/Qt6MultimediaQuick \
   -DQt6MultimediaWidgets_DIR=$QT/lib/cmake/Qt6MultimediaWidgets \
   -DQt6Network_DIR=$QT/lib/cmake/Qt6Network \
   -DQt6OpenGL_DIR=$QT/lib/cmake/Qt6OpenGL \
   -DQt6OpenGLExtensions_DIR=$QT/lib/cmake/Qt6OpenGLExtensions \
   -DQt6PlatformCompositorSupport_DIR=$QT/lib/cmake/Qt6PlatformCompositorSupport \
   -DQt6Qml_DIR=$QT/lib/cmake/Qt6Qml \
   -DQt6QmlDevTools_DIR=$QT/lib/cmake/Qt6QmlDevTools \
   -DQt6QmlImportScanner_DIR=$QT/lib/cmake/Qt6QmlImportScanner \
   -DQt6QmlModels_DIR=$QT/lib/cmake/Qt6QmlModels \
   -DQt6Quick_DIR=$QT/lib/cmake/Qt6Quick \
   -DQt6QuickCompiler_DIR=$QT/lib/cmake/Qt6QuickCompiler \
   -DQt6QuickParticles_DIR=$QT/lib/cmake/Qt6QuickParticles \
   -DQt6QuickShapes_DIR=$QT/lib/cmake/Qt6QuickShapes \
   -DQt6QuickWidgets_DIR=$QT/lib/cmake/Qt6QuickWidgets \
   -DQt6ServiceSupport_DIR=$QT/lib/cmake/Qt6ServiceSupport \
   -DQt6ThemeSupport_DIR=$QT/lib/cmake/Qt6ThemeSupport \
   -DQt6WebSockets_DIR=$QT/lib/cmake/Qt6WebSockets \
   -DQt6Widgets_DIR=$QT/lib/cmake/Qt6Widgets \
   -DQt6Xml_DIR=$QT/lib/cmake/Qt6Xml \
   -DQt6Zlib_DIR=$QT/lib/cmake/Qt6Zlib


cmake --build .
~/libs/qt6-wasm/qtbase/bin/qt-cmake -GNinja $SCORE_DIR \
   -DCMAKE_C_FLAGS="-O3 -g0" \
   -DCMAKE_CXX_FLAGS="-O3 -g0" \
   -DCMAKE_CXX_STANDARD=17 \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_UNITY_BUILD=1 \
   -DQT_VERSION="Qt6;6.3" \
   -DOSSIA_PCH=0 \
   -DSCORE_PCH=0


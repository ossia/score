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
   -DQt5_DIR=$QT/lib/cmake/Qt5 \
   -DQt5Core_DIR=$QT/lib/cmake/Qt5Core \
   -DQt5DeviceDiscoverySupport_DIR=$QT/lib/cmake/Qt5DeviceDiscoverySupport \
   -DQt5EdidSupport_DIR=$QT/lib/cmake/Qt5EdidSupport \
   -DQt5EglSupport_DIR=$QT/lib/cmake/Qt5EglSupport \
   -DQt5EventDispatcherSupport_DIR=$QT/lib/cmake/Qt5EventDispatcherSupport \
   -DQt5FbSupport_DIR=$QT/lib/cmake/Qt5FbSupport \
   -DQt5FontDatabaseSupport_DIR=$QT/lib/cmake/Qt5FontDatabaseSupport \
   -DQt5Gui_DIR=$QT/lib/cmake/Qt5Gui \
   -DQt5Multimedia_DIR=$QT/lib/cmake/Qt5Multimedia \
   -DQt5MultimediaQuick_DIR=$QT/lib/cmake/Qt5MultimediaQuick \
   -DQt5MultimediaWidgets_DIR=$QT/lib/cmake/Qt5MultimediaWidgets \
   -DQt5Network_DIR=$QT/lib/cmake/Qt5Network \
   -DQt5OpenGL_DIR=$QT/lib/cmake/Qt5OpenGL \
   -DQt5OpenGLExtensions_DIR=$QT/lib/cmake/Qt5OpenGLExtensions \
   -DQt5PlatformCompositorSupport_DIR=$QT/lib/cmake/Qt5PlatformCompositorSupport \
   -DQt5Qml_DIR=$QT/lib/cmake/Qt5Qml \
   -DQt5QmlDevTools_DIR=$QT/lib/cmake/Qt5QmlDevTools \
   -DQt5QmlImportScanner_DIR=$QT/lib/cmake/Qt5QmlImportScanner \
   -DQt5QmlModels_DIR=$QT/lib/cmake/Qt5QmlModels \
   -DQt5Quick_DIR=$QT/lib/cmake/Qt5Quick \
   -DQt5QuickCompiler_DIR=$QT/lib/cmake/Qt5QuickCompiler \
   -DQt5QuickParticles_DIR=$QT/lib/cmake/Qt5QuickParticles \
   -DQt5QuickShapes_DIR=$QT/lib/cmake/Qt5QuickShapes \
   -DQt5QuickWidgets_DIR=$QT/lib/cmake/Qt5QuickWidgets \
   -DQt5ServiceSupport_DIR=$QT/lib/cmake/Qt5ServiceSupport \
   -DQt5ThemeSupport_DIR=$QT/lib/cmake/Qt5ThemeSupport \
   -DQt5WebSockets_DIR=$QT/lib/cmake/Qt5WebSockets \
   -DQt5Widgets_DIR=$QT/lib/cmake/Qt5Widgets \
   -DQt5Xml_DIR=$QT/lib/cmake/Qt5Xml \
   -DQt5Zlib_DIR=$QT/lib/cmake/Qt5Zlib


cmake --build .


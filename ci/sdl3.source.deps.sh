#!/bin/bash -eux

# SDL3 from source, for distributions that do not package it yet. Sourced, not
# executed, so that $SUDO from common.setup.sh is in scope - hence no `exit`
# anywhere in here, which would take the calling deps script down with it.
#
# Ubuntu jammy (22.04), noble (24.04), lunar and oracular ship no libsdl3 at all;
# Debian trixie/sid and Ubuntu plucky/questing do package it and use libsdl3-dev
# instead. Without this, find_package(SDL3) finds nothing, OSSIA_ENABLE_SDL goes
# off, and the build silently loses the joystick protocol and the SDL audio
# engine.
#
# Version tracks SDL_VERSION in ossia/sdk's common/versions.sh. The subsystem set
# matches the SDK's sdl.sh and the flatpak module: score only needs joystick,
# gamepad, haptic, sensor and audio, so video and its dependencies stay off -
# which is also why SDL_UNIX_CONSOLE_BUILD is needed, as SDL3 otherwise refuses
# to configure with both X11 and Wayland disabled.

if pkg-config --exists sdl3; then
  echo "SDL3 $(pkg-config --modversion sdl3) already available, skipping source build"
else
  (
    SDL3_VERSION=3.4.14
    SDL3_SRC="SDL3-$SDL3_VERSION"

    wget -nv "https://github.com/libsdl-org/SDL/releases/download/release-$SDL3_VERSION/$SDL3_SRC.tar.gz"
    tar xzf "$SDL3_SRC.tar.gz"
    rm "$SDL3_SRC.tar.gz"

    cmake -S "$SDL3_SRC" -B sdl3-build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DBUILD_SHARED_LIBS=ON \
      -DSDL_JOYSTICK=ON \
      -DSDL_HAPTIC=ON \
      -DSDL_HIDAPI=ON \
      -DSDL_HIDAPI_JOYSTICK=ON \
      -DSDL_POWER=ON \
      -DSDL_SENSOR=ON \
      -DSDL_AUDIO=ON \
      -DSDL_VIDEO=OFF \
      -DSDL_RENDER=OFF \
      -DSDL_CAMERA=OFF \
      -DSDL_GPU=OFF \
      -DSDL_DIALOG=OFF \
      -DSDL_TRAY=OFF \
      -DSDL_X11=OFF \
      -DSDL_WAYLAND=OFF \
      -DSDL_UNIX_CONSOLE_BUILD=ON \
      -DSDL_TESTS=OFF \
      -DSDL_TEST_LIBRARY=OFF \
      -DSDL_EXAMPLES=OFF \
      -DSDL_INSTALL_CPACK=OFF

    cmake --build sdl3-build --parallel
    $SUDO cmake --install sdl3-build
    $SUDO ldconfig

    rm -rf sdl3-build "$SDL3_SRC"
  )
fi

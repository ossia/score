{ stdenv
, lib
, fetchFromGitHub
, cmake
, ninja
, pkg-config
, qttools
, wrapQtAppsHook
, alsa-lib
, boost185
# , faust
, git
, ffmpeg
, fftw
, flac
, gnutls
, lame
, libcoap
, libjack2
, libopus
, libsndfile
, libvorbis
, lilv
, lv2
, mpg123
, pipewire
, portaudio
, portmidi
, libsamplerate
, qtbase
, qtdeclarative
, qtscxml
, qtserialport
, qtshadertools
, qtsvg
, qtwayland
, qtwebsockets
, rapidfuzz-cpp
, re2
, suil
}:

# TODO: figure out LLVM jit
# assert lib.versionAtLeast llvm.version "15";

stdenv.mkDerivation (finalAttrs: {
  pname = "ossia-score";
  version = "devel";
  src = ../.;
  nativeBuildInputs = [ cmake ninja pkg-config qttools wrapQtAppsHook ];

  # TODO: figure out if we want avahi / bluez / speex / SDL2 (needed for joystick support) by default
  # what about leapmotion?
  buildInputs = [
    alsa-lib
    boost185
    # faust
    ffmpeg
    fftw
    flac
    git
    gnutls
    lame
    libcoap
    libjack2
    libopus
    libsamplerate
    libsndfile
    libvorbis
    lilv
    lv2
    mpg123
    pipewire
    portaudio
    portmidi
    qtbase
    qtdeclarative
    qtserialport
    qtscxml
    qtshadertools
    qtsvg
    qtwayland
    qtwebsockets
    rapidfuzz-cpp
    re2
    suil
  ];

  cmakeFlags = [
    "-Wno-dev"

    "-DSCORE_DEPLOYMENT_BUILD=1"
    "-DSCORE_STATIC_PLUGINS=1"
    "-DSCORE_FHS_BUILD=1"
    "-DCMAKE_UNITY_BUILD=1"
    "-DCMAKE_SKIP_RPATH=ON"
    "-DOSSIA_USE_SYSTEM_LIBRARIES=1"

    "-DCMAKE_CXX_FLAGS=-O0"
    "-DCMAKE_EXE_LINKER_FLAGS=-O0"

    "-DLilv_INCLUDE_DIR=${lilv.dev}/include/lilv-0"
    "-DLilv_LIBRARY=${lilv}/lib/liblilv-0.so"

    "-DSuil_INCLUDE_DIR=${suil}/include/suil-0"
    "-DSuil_LIBRARY=${suil}/lib/libsuil-0.so"
  ];

  # Ossia dlopen's these at runtime, refuses to start without them
  env.NIX_LDFLAGS = toString [
    "-llilv-0"
    "-lsuil-0"
  ];

  installPhase = ''
    runHook preInstall

    cmake -DCMAKE_INSTALL_DO_STRIP=1 -DCOMPONENT=OssiaScore -P cmake_install.cmake

    runHook postInstall
  '';

  meta = with lib; {
    homepage = "https://ossia.io/score/about.html";
    description = "A sequencer for audio-visual artists, designed to enable the creation of interactive shows, museum installations, intermedia digital artworks, interactive music and more";
    # TODO: this should work for darwin too
    platforms = platforms.linux;
    license = licenses.gpl3Only;
    maintainers = with maintainers; [ minijackson jcelerier ];
  };
})


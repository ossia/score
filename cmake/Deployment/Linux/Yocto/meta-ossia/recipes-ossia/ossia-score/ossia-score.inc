SUMMARY = "ossia score - an interactive sequencer for audio-visual artists"
DESCRIPTION = "ossia score is a sequencer for audio-visual artists, designed to \
enable the creation of interactive shows, museum installations, intermedia \
digital artworks, interactive music and more."
HOMEPAGE = "https://ossia.io"
BUGTRACKER = "https://github.com/ossia/score/issues"
SECTION = "multimedia"

# score itself is GPL-3.0-only. The source archive additionally vendors a large
# third-party tree under 3rdparty/ and src/addons/*/3rdparty/ with mixed
# licensing (BSD/MIT/Apache/BSL, and the Steinberg VST3 SDK under its dual
# GPLv3/proprietary terms). Before shipping an image commercially, run
# `bitbake -c populate_lic ossia-score` and audit that output; the declaration
# here covers score's own sources only.
LICENSE = "GPL-3.0-only"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=c8bf0f99367f566cced5973a2aa89d68"

# Support files shared by both the release-tarball and the externalsrc recipe.
# externalsrc filters SRC_URI down to its local (file://) entries and keeps
# do_unpack, so these land in UNPACKDIR either way.
SRC_URI = " \
    file://ossia-score-launch \
    file://ossia-score-init \
    file://ossia-score.service \
    file://95-ossia-score-rt.conf \
"

inherit qt6-cmake pkgconfig systemd features_check

# KFR refuses to build on non-x86 without Clang, and score's own CI/AppImage
# use it too. toolchain/clang.bbclass is in oe-core; no meta-clang needed.
TOOLCHAIN = "clang"

# score hard-requires Qt Gui/Widgets/OpenGL at find_package() time (see
# CMakeLists.txt "find_package(${QT_VERSION} REQUIRED COMPONENTS ...)"), even
# for headless use -- --no-gui is a runtime flag, not a build configuration.
REQUIRED_DISTRO_FEATURES = "opengl"

DEPENDS = " \
    qtbase \
    qtdeclarative \
    qtscxml \
    qtshadertools \
    qtsvg \
    qtdeclarative-native \
    qtshadertools-native \
    boost \
    alsa-lib \
    ffmpeg \
    libsndfile1 \
    fftw \
    avahi \
    portaudio-v19 \
    re2 \
    snappy \
    spdlog \
    magic-enum \
    rapidfuzz-cpp \
    openssl \
    zlib \
    udev \
"

PACKAGECONFIG ??= "jack bluez websockets serialport quick3d gpsd hdf5 sdl coap \
                   lv2 faust ysfx \
                   ${@bb.utils.contains_any('TARGET_ARCH', 'aarch64 x86_64', 'jit', '', d)} \
                   ${@bb.utils.filter('DISTRO_FEATURES', 'wayland x11', d)}"

PACKAGECONFIG[pipewire]   = ",,pipewire"
PACKAGECONFIG[jack]       = ",,jack"
PACKAGECONFIG[bluez]      = ",,bluez5"
PACKAGECONFIG[websockets] = ",,qtwebsockets"
PACKAGECONFIG[serialport] = ",,qtserialport"
PACKAGECONFIG[quick3d]    = ",,qtquick3d"
PACKAGECONFIG[gpsd]       = ",,gpsd"
PACKAGECONFIG[hdf5]       = ",,hdf5"
PACKAGECONFIG[sdl]        = ",,libsdl2"
PACKAGECONFIG[coap]       = ",,libcoap"
PACKAGECONFIG[x11]        = ",,libx11 libxext libxcomposite libxrandr"
# 4th field: the QPA plugin is dlopen'ed, so it needs to be a runtime dependency
# too or `ossia-score-launch wayland` finds no platform plugin at startup.
PACKAGECONFIG[wayland]    = ",,qtwayland wayland,qtwayland-plugins"

# Explicit LLVM_DIR: without it find_package(LLVM) resolves the *native* LLVM
# from recipe-sysroot-native and the target link fails on -lclang.
# Runtime needs clang's resource headers and a C++ header set to compile against.
PACKAGECONFIG[jit] = "-DLLVM_DIR=${STAGING_LIBDIR}/cmake/llvm,,clang,libclang clang libstdc++-dev"

# The JIT genuinely needs headers at runtime, which dev-deps flags.
INSANE_SKIP:${PN} += "${@bb.utils.contains('PACKAGECONFIG', 'jit', 'dev-deps', '', d)}"

# Without these the plugins self-disable silently.
PACKAGECONFIG[lv2]        = ",,lv2kit,lv2kit"
PACKAGECONFIG[faust]      = ",,faust,faust faustlibraries"
PACKAGECONFIG[ysfx]       = ",,ysfx"

# score-addon-onnx FetchContent's a PREBUILT x86_64 onnxruntime tarball and
# score-addon-ultraleap pulls a proprietary SDK; both fetch during configure,
# which cannot work under BitBake's offline do_configure, and neither
# cross-compiles. This list also filters src/addons (see src/addons/CMakeLists.txt,
# which skips any subdir named in SCORE_DISABLED_PLUGINS). Everything else --
# every plugin and every remaining addon -- is built.
SCORE_DISABLED_PLUGINS ?= "score-addon-onnx,score-addon-ultraleap"

SCORE_DISABLED_PLUGINS .= "${@bb.utils.contains('PACKAGECONFIG', 'jit', '', ',score-plugin-jit', d)}"

# The distro-packaging configuration, the same one used by
# ci/debian.trixie-system.build.sh, ci/nix.build.nix and the Flatpak manifest:
# system libraries where available, FHS layout, one statically-linked binary.
#
# SCORE_USE_SYSTEM_LIBRARIES is a *preference*, not a requirement: libossia's
# cmake/deps/*.cmake fall back to the vendored copy when a package is not found,
# so the many header-only dependencies with no OE recipe (fmt, rapidfuzz,
# rubberband, zita, libsamplerate, ...) resolve to the in-tree versions. It also
# suppresses the faustlibs ExternalProject git fetch in
# src/plugins/score-plugin-faust/CMakeLists.txt, which would otherwise need the
# network at build time.
# OE's cmake.bbclass leaves CMAKE_BUILD_TYPE unset -- it supplies optimisation
# through CFLAGS instead -- but score expects Release to be set explicitly (its
# CMakeLists warns "No build type defined"), and the bundled VST3 SDK will not
# compile without it: base/source/fdebug.h #errors unless one of DEVELOPMENT,
# RELEASE, _DEBUG or NDEBUG is defined. The generated toolchain file sets
# CMAKE_CXX_FLAGS_RELEASE to just "-DNDEBUG" with no -O flag, so this adds the
# define without overriding FULL_OPTIMIZATION above. The Flatpak manifest calls
# out the same requirement.
EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DSCORE_USE_SYSTEM_LIBRARIES=1 \
    -DOSSIA_USE_SYSTEM_LIBRARIES=1 \
    -DSCORE_DEPLOYMENT_BUILD=1 \
    -DSCORE_STATIC_PLUGINS=1 \
    -DSCORE_FHS_BUILD=1 \
    -DBUILD_SHARED_LIBS=0 \
    -DCMAKE_SKIP_RPATH=ON \
    -DCMAKE_UNITY_BUILD=1 \
    -DSCORE_PCH=0 \
    -DBOOST_ROOT=${STAGING_INCDIR} \
    -DSCORE_DISABLED_PLUGINS=${SCORE_DISABLED_PLUGINS} \
"

# Performance: score is a soft-realtime audio/video engine, so it is built -O3
# rather than the -O2 default. Debug flags are left to OE so that the -dbg
# package still works; they do not affect the generated code.
FULL_OPTIMIZATION = "-O3 ${DEBUG_LEVELFLAG} -fno-semantic-interposition"

# Link-time optimisation. Off by default: score links a single ~70 MB static
# binary from a unity build, and an LTO link of that size needs a great deal of
# RAM on the build host. Set OSSIA_SCORE_LTO = "1" in local.conf to enable.
OSSIA_SCORE_LTO ?= "0"
EXTRA_OECMAKE += "${@bb.utils.contains('OSSIA_SCORE_LTO', '1', '-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON', '', d)}"

# score links everything into one binary, so the build is memory-hungry rather
# than parallel-hungry; unity builds make individual TUs very large.
PARALLEL_MAKE ?= "-j 4"

SYSTEMD_SERVICE:${PN} = "ossia-score.service"
SYSTEMD_AUTO_ENABLE = "disable"

do_install:append() {
    # xtensor is vendored through add_subdirectory, so its own install() rules
    # run as part of score's and drop Jupyter/xeus integration files -- into
    # ${prefix}/etc rather than ${sysconfdir}, at that. Nothing here consumes
    # them. (Its headers land in ${includedir} and go to the -dev package, which
    # the image does not install, so those are left alone.)
    rm -rf ${D}${datadir}/xeus-cpp ${D}${prefix}/etc/xeus-cpp
    rmdir --ignore-fail-on-non-empty ${D}${prefix}/etc 2>/dev/null || true

    install -d ${D}${bindir}
    install -m 0755 ${UNPACKDIR}/ossia-score-launch ${D}${bindir}/ossia-score-launch
    install -m 0755 ${UNPACKDIR}/ossia-score-init ${D}${bindir}/ossia-score-init

    install -d ${D}${sysconfdir}/security/limits.d
    install -m 0644 ${UNPACKDIR}/95-ossia-score-rt.conf \
        ${D}${sysconfdir}/security/limits.d/95-ossia-score-rt.conf

    if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
        install -d ${D}${systemd_system_unitdir}
        install -m 0644 ${UNPACKDIR}/ossia-score.service \
            ${D}${systemd_system_unitdir}/ossia-score.service
    fi
}

FILES:${PN} += " \
    ${datadir}/applications \
    ${datadir}/icons \
    ${datadir}/metainfo \
    ${datadir}/pixmaps \
    ${sysconfdir}/security/limits.d \
"

# Qt platform integration is dlopen'ed, so it has to be pulled in explicitly.
# eglfs is what the launcher uses when there is no display server.
RDEPENDS:${PN} += " \
    qtbase-plugins \
    qtdeclarative-qmlplugins \
"

RRECOMMENDS:${PN} += "alsa-utils"

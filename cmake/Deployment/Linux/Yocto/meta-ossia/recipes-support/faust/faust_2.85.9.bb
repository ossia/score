SUMMARY = "FAUST - Functional Audio Stream, compiler library"
DESCRIPTION = "libfaust compiles FAUST DSP source to machine code at runtime \
through LLVM. score-plugin-faust uses it to let users write and edit DSP live; \
score's cmake/modules/FindFaust.cmake looks for faust/dsp/llvm-dsp.h and \
libfaust.so, both of which this recipe installs."
HOMEPAGE = "https://faust.grame.fr"
SECTION = "libs"

LICENSE = "LGPL-2.1-or-later"
LIC_FILES_CHKSUM = "file://COPYING.txt;md5=d23dc65dc39305c10b858dabaeb8acc6"

SRC_URI = " \
    git://github.com/grame-cncm/faust;protocol=https;branch=master-dev;nobranch=1 \
    file://0001-cmake-do-not-hardcode-usr-local-include.patch \
"
SRCREV = "3c44e5cb64be06f8ac7c025f1633178276f35d8a"

# S at the checkout root, not build/: LIC_FILES_CHKSUM, patchdir and
# -ffile-prefix-map are all relative to it.
S = "${UNPACKDIR}/${BP}"
OECMAKE_SOURCEPATH = "${S}/build"

DEPENDS = "clang llvm zlib"

inherit cmake pkgconfig

# Flags follow score's Flatpak manifest (Flatpak/modules/faust.yaml).
# USE_LLVM_CONFIG=OFF: OE's llvm-config cross wrapper emits nothing without
# NEXT_LLVM_CONFIG, so faust must use find_package(LLVM CONFIG) instead.
EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=ON \
    -DINCLUDE_OSC=0 \
    -DINCLUDE_HTTP=0 \
    -DINCLUDE_EXECUTABLE=0 \
    -DINCLUDE_STATIC=0 \
    -DINCLUDE_DYNAMIC=1 \
    -DINCLUDE_WASM_GLUE=0 \
    -DLINK_LLVM_STATIC=0 \
    -DINCLUDE_LLVM=1 \
    -DUSE_LLVM_CONFIG=OFF \
    -DLLVM_BACKEND='COMPILER;DYNAMIC' \
    -DC_BACKEND=COMPILER \
    -DCPP_BACKEND=COMPILER \
    -DCODEBOX_BACKEND=OFF \
    -DCMAJOR_BACKEND=OFF \
    -DCSHARP_BACKEND=OFF \
    -DFIR_BACKEND=OFF \
    -DINTERP_BACKEND=OFF \
    -DJAVA_BACKEND=OFF \
    -DJAX_BACKEND=OFF \
    -DJSFX_BACKEND=OFF \
    -DJULIA_BACKEND=OFF \
    -DOLDCPP_BACKEND=OFF \
    -DRUST_BACKEND=OFF \
    -DSDF3_BACKEND=OFF \
    -DVHDL_BACKEND=OFF \
    -DWASM_BACKEND=OFF \
    -DEXTRA_CXX_FLAGS=-DNDEBUG \
"

# The architecture tree (43 MB, with prebuilt arm/ios libsndfile binaries) and
# the 114 faust2* bash wrappers drive the faust CLI, which we do not build.
# Removing datadir also leaves it free for faustlibraries.
do_install:append() {
    rm -rf ${D}${datadir}/faust
    rm -rf ${D}${bindir}
    rm -f ${D}${libdir}/ios-libsndfile.a
}

RDEPENDS:${PN} += "faustlibraries"

# faust overwrites CMAKE_CXX_FLAGS_RELEASE, dropping OE's -DNDEBUG;
# EXTRA_CXX_FLAGS is its own injection point.

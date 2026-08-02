SUMMARY = "ysfx - JSFX (REAPER effects) hosting library"
DESCRIPTION = "An implementation of the JSFX scripting language used by \
REAPER's built-in effects, as a host library. score-plugin-ysfx uses it to run \
.jsfx scripts as audio processes."
HOMEPAGE = "https://github.com/jcelerier/ysfx"
SECTION = "libs"

LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=3b83ef96387f14655fc854ddc3c6bd57"

# ossia's fork, same branch ossia/sdk pins.
SRC_URI = "gitsm://github.com/jcelerier/ysfx;protocol=https;branch=ossia/2026-01"
SRCREV = "b7565b20b48600f1f79093ee706662d13c533883"
PV = "0.0+git"

DEPENDS = "libsndfile1"

inherit cmake pkgconfig

# Flags follow ossia/sdk (Linux/ysfx.sh); YSFX_PLUGIN would pull in JUCE.
EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DYSFX_PLUGIN=OFF \
    -DYSFX_DEEP_STRIP=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
"

# The ysfx-s name is score's API-version marker: FindYsfx.cmake and
# __has_include(<ysfx-s.h>) select the 4-argument slider API this branch
# provides. ossia/sdk renames identically (Linux/ysfx.sh).
do_install:append() {
    mv ${D}${includedir}/ysfx.h ${D}${includedir}/ysfx-s.h
    mv ${D}${libdir}/libysfx.a ${D}${libdir}/libysfx-s.a
}

ALLOW_EMPTY:${PN} = "1"

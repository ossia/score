SUMMARY = "The LV2 plugin standard and its reference implementation libraries"
DESCRIPTION = "lv2kit bundles the LV2 specification together with serd, sord, \
sratom, zix, lilv, suil and pugl -- the host-side libraries score-plugin-lv2 \
needs to discover, load and embed LV2 plugins and their UIs. Upstream ships \
them as one meson project with the individual libraries as subprojects, which \
is also how ossia/sdk builds them (Linux/lv2.sh)."
HOMEPAGE = "https://github.com/lv2/lv2kit"
SECTION = "libs"

LICENSE = "ISC & 0BSD"
LIC_FILES_CHKSUM = " \
    file://LICENSES/ISC.txt;md5=c17d0f531d833bea1a6c6823ba18c242 \
    file://LICENSES/0BSD.txt;md5=02e89155647b79737003bfeeeaf17d5d \
"

# gitsm: the libraries are submodules meson consumes as subprojects.
SRC_URI = "gitsm://github.com/lv2/lv2kit;protocol=https;branch=main"
SRCREV = "a99b4f4a4177e04b13bd384fbdcce59a721d6370"
PV = "1.18.10+git"

inherit meson pkgconfig

# forcefallback, as ossia/sdk does: no OE recipes exist for serd/sord/sratom/zix.
EXTRA_OEMESON = " \
    -Ddocs=disabled \
    -Dtests=disabled \
    -Dtools=disabled \
    --wrap-mode=forcefallback \
"

# ${libdir}/lv2, not ${datadir}: lilv walks this at runtime for the core
# ontologies. Missing them, every plugin fails to instantiate.
FILES:${PN} += "${libdir}/lv2"

# lilv's python binding would pull a python3 runtime into the image.
do_install:append() {
    rm -rf ${D}${libdir}/python*
}

BBCLASSEXTEND = "native nativesdk"

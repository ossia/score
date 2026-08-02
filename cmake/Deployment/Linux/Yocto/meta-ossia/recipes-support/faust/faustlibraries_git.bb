SUMMARY = "The FAUST DSP libraries"
DESCRIPTION = "The .lib files libfaust reads when compiling a patch -- \
stdfaust.lib and everything it pulls in. Data only, no compilation."
HOMEPAGE = "https://github.com/grame-cncm/faustlibraries"
SECTION = "libs"

# No top-level licence file; terms are per-file. basics.lib is representative.
LICENSE = "LGPL-2.1-or-later & MIT"
LIC_FILES_CHKSUM = " \
    file://basics.lib;beginline=16;endline=40;md5=8226c9d347e636b36519ac04622b637e \
    file://licenses/stk-4.3.0.md;md5=603cf1260d37836db42b7dd30a2c5536 \
"

SRC_URI = "git://github.com/grame-cncm/faustlibraries;protocol=https;branch=master;nobranch=1"

# Same revision score-plugin-faust fetches; they must move together.
SRCREV = "730eff6dc336973553829235e0b31b24c47a2f69"
PV = "0.0+git"

inherit allarch

do_install() {
    install -d ${D}${datadir}/faust
    install -m 0644 ${S}/*.lib ${D}${datadir}/faust/
}

FILES:${PN} = "${datadir}/faust"

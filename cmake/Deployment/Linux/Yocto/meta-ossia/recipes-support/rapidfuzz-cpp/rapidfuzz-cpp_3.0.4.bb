SUMMARY = "Rapid fuzzy string matching in C++"
DESCRIPTION = "Header-only C++ library for fast approximate string matching, \
using the Levenshtein distance and related metrics."
HOMEPAGE = "https://github.com/rapidfuzz/rapidfuzz-cpp"
SECTION = "libs"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=377d4340f3278257d178cafe2c22cfc8"

# Pinned to the version score vendors in
# 3rdparty/libossia/3rdparty/rapidfuzz-cpp rather than upstream's latest, so
# that the system package and the vendored fallback are the same API.
#
# This recipe exists because libossia's cmake/deps/rapidfuzz.cmake calls
# find_package(rapidfuzz CONFIG REQUIRED) when OSSIA_USE_SYSTEM_LIBRARIES is on,
# which aborts configure before the vendored fallback below it can run -- and
# rapidfuzz-cpp is not packaged in oe-core or meta-openembedded. score master
# has a per-package OSSIA_USE_SYSTEM_<pkg> override that would sidestep this;
# the 3.8.2 release does not.
#
# git rather than the release tarball: OE recipe QA rejects GitHub's
# auto-generated archives as unstable, since they can be regenerated with a
# different hash. SRCREV is the commit that tag v3.0.4 points at.
SRC_URI = "git://github.com/rapidfuzz/rapidfuzz-cpp;protocol=https;branch=main"
SRCREV = "10426d24cd7479df0fe8c78b17877e756e1c3cd5"

inherit cmake

EXTRA_OECMAKE = "-DRAPIDFUZZ_BUILD_TESTING=OFF -DRAPIDFUZZ_ENABLE_LINTERS=OFF"

# Header-only: everything lands in the -dev package and there is no runtime
# component, so the main package is legitimately empty.
ALLOW_EMPTY:${PN} = "1"
RDEPENDS:${PN}-dev = ""

require ossia-score.inc

# Builds the score checkout that this layer ships inside, rather than a release
# tarball. This is the recipe you normally want: development targets master, and
# a gitsm:// fetch of ossia/score alone would silently produce a build with no
# addons (they are ~20 separate repositories that ci/common.deps.sh clones).
#
# Higher PV than ossia-score_3.8.2.bb, so this is what bitbake picks by default.
# Use PREFERRED_VERSION_ossia-score = "3.8.2" for a pinned, reproducible build.
PV = "3.8.2+git"

# The score tree is seven levels up from this recipe:
#   <score>/cmake/Deployment/Linux/Yocto/meta-ossia/recipes-ossia/ossia-score
# Override in local.conf to build a different checkout.
SCORE_SRC_ROOT ?= "${@os.path.normpath(os.path.join(d.getVar('THISDIR'), '../../../../../../..'))}"

inherit externalsrc

EXTERNALSRC = "${SCORE_SRC_ROOT}"
EXTERNALSRC_BUILD = "${WORKDIR}/build"

# externalsrc has no do_fetch, so submodules and addons have to already be
# there. Fail with something readable rather than a CMake error 40 lines deep.
do_configure:prepend() {
    if [ ! -f "${S}/3rdparty/libossia/CMakeLists.txt" ]; then
        bbfatal "No libossia in ${S}/3rdparty. Run: git submodule update --init --recursive"
    fi
    if [ ! -d "${S}/src/addons/score-addon-avnd" ] && [ ! -e "${S}/src/addons/iscore-addon-network" ]; then
        bbwarn "No addons in ${S}/src/addons -- run ci/common.deps.sh LINUX to clone them."
    fi
}

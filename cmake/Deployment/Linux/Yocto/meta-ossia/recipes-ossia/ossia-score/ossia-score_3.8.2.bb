require ossia-score.inc

# The release source archive, not a git checkout. score has 39 submodules and
# libossia 44 more, plus ~20 addon repositories that ci/common.deps.sh clones
# separately -- a gitsm:// fetch would miss the addons entirely. The archive
# produced by ci/tarball.build.sh already contains all of it with the .git
# directories stripped, and is published (and GPG-signed) on every release.
# The support files (launcher, unit, limits) come from ossia-score.inc.
SRC_URI += "https://github.com/ossia/score/releases/download/v${PV}/ossia.score-${PV}-src.tar.xz"
SRC_URI[sha256sum] = "f12c85ec96689b73e92b954589c39187a5ae9cfdf95db08d88c0e5f4dd862259"

S = "${UNPACKDIR}/ossia-score-${PV}"

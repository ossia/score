#!/bin/bash -eux
# Dependencies of the coverage job (.github/workflows/coverage.yml), which runs
# in an ubuntu:26.10 container.
#
# - LLVM / clang from apt.llvm.org: the JIT plug-in builds against it, so the
#   process sweeps also run the JIT, Avendish and C++ texture generator
#   processes. apt.llvm.org has no 26.10 repository; its 26.04 one installs
#   cleanly on 26.10, which only carries newer versions of the libraries it
#   links to. The newest release branch it carries is taken (LLVM_VERSION
#   overrides it), not the unversioned snapshot repository, which follows LLVM's
#   main branch. It is the only LLVM development package set installed: the
#   compiler, llvm-cov and the CMake directories of the build step come from it,
#   through $GITHUB_ENV.
# - Qt, Mesa and the rest from the distribution, through ci/ubuntu.2604.deps.sh,
#   whose package names 26.10 shares.
# - CMake and gcovr from PyPI, where their releases land first, at the versions
#   and hashes of ci/coverage.requirements.txt.

source ci/common.setup.sh

LLVM_APT_URL=https://apt.llvm.org/resolute
LLVM_KEY_FINGERPRINT=6084F3CF814B57C1CF12EFD515CF4D18AF4F7421

echo 'debconf debconf/frontend select Noninteractive' | $SUDO debconf-set-selections

$SUDO apt-get update -qq
$SUDO apt-get install -y --no-install-recommends ca-certificates curl gpg python3-venv

# >>> LLVM
keyring=/etc/apt/keyrings/apt.llvm.org.asc
$SUDO install -d -m 0755 /etc/apt/keyrings
curl -fsSL --retry 3 https://apt.llvm.org/llvm-snapshot.gpg.key | $SUDO tee "$keyring" >/dev/null
if ! gpg --show-keys --with-colons "$keyring" | grep -q "^fpr:::::::::${LLVM_KEY_FINGERPRINT}:"; then
  echo "apt.llvm.org key does not have the fingerprint ${LLVM_KEY_FINGERPRINT}" >&2
  exit 1
fi
if [ -z "${LLVM_VERSION:-}" ]; then
  LLVM_VERSION=$(./ci/coverage.llvm-version.sh "$LLVM_APT_URL")
fi
echo "LLVM ${LLVM_VERSION} from ${LLVM_APT_URL}"
$SUDO tee /etc/apt/sources.list.d/apt.llvm.org.sources >/dev/null <<EOF
Types: deb
URIs: ${LLVM_APT_URL}/
Suites: llvm-toolchain-resolute-${LLVM_VERSION}
Components: main
Signed-By: ${keyring}
EOF
# <<< LLVM

# libclang-rt holds the ORC runtime the JIT uses for its native platform.
PKGS="clang-${LLVM_VERSION} lld-${LLVM_VERSION} llvm-${LLVM_VERSION} \
      llvm-${LLVM_VERSION}-dev libclang-${LLVM_VERSION}-dev \
      libclang-rt-${LLVM_VERSION}-dev" \
  ./ci/ubuntu.2604.deps.sh

# Software OpenGL (llvmpipe) and a virtual X display (xvfb) so the SCORE_TESTING
# ctest suite, GUI-labelled tests included, can run.
$SUDO apt-get install -y --no-install-recommends \
  libgl1-mesa-dri libglx-mesa0 mesa-utils xvfb \
  qml6-module-qtqml-workerscript

# V4L2 loader tests dlopen libv4l2.so.0, including with no capture hardware.
$SUDO apt-get install -y --no-install-recommends libv4l-0t64

# >>> software Vulkan (lavapipe)
# So that the gfx tests run on Vulkan as well as on OpenGL, and the ones
# comparing two backends run at all. vulkan-tools is for the device summary
# coverage.build.sh logs.
$SUDO apt-get install -y --no-install-recommends \
  mesa-vulkan-drivers libvulkan1 vulkan-tools
# <<< software Vulkan (lavapipe)

# >>> validation audio plug-ins
# The process sweeps (tests/nodes/{Processes,ProcessViews}.cpp) run the CLAP,
# LV2, VST 2 and VST 3 hosts with DPF-Plugins' MaFreeverb and Kars, and LV2's
# eg-amp and eg-midigate (tests/nodes/ValidationPlugins.hpp). Pinned so that a
# plug-in update cannot change what the sweeps test without a commit here: these
# are the versions of the release pocket of 26.10, which is never rewritten, and
# moves to old-releases.ubuntu.com with the rest of the release (see the first
# step of .github/workflows/coverage.yml). Should a version not be there, the
# archive's own is installed, and its version and checksums are printed as a
# warning so that a change in what the sweeps ran is visible.
VALIDATION_PLUGINS="dpf-plugins-clap=1.7+ds-3build2 dpf-plugins-lv2=1.7+ds-3build2 \
  dpf-plugins-vst=1.7+ds-3build2 dpf-plugins-vst3=1.7+ds-3build2 \
  lv2-examples=1.18.10-3"
# shellcheck disable=SC2086
if ! $SUDO apt-get install -y --no-install-recommends $VALIDATION_PLUGINS; then
  unpinned=$(for p in $VALIDATION_PLUGINS; do echo "${p%%=*}"; done)
  # shellcheck disable=SC2086
  $SUDO apt-get install -y --no-install-recommends $unpinned
  for p in $VALIDATION_PLUGINS; do
    installed=$(dpkg-query -W -f='${Version}' "${p%%=*}")
    [ "$installed" = "${p#*=}" ] && continue
    echo "::warning::${p%%=*} ${installed} is installed instead of ${p#*=}; the process sweeps run it."
    dpkg-query -L "${p%%=*}" | grep -E '\.(so|clap)$' | xargs -r sha256sum
  done
fi
# <<< validation audio plug-ins

# >>> CMake and gcovr
# /usr/local/bin comes before /usr/bin, where ubuntu.2604.deps.sh puts the
# distribution's CMake.
$SUDO python3 -m venv /opt/coverage-tools
$SUDO /opt/coverage-tools/bin/pip install --no-cache-dir --require-hashes \
  -r ci/coverage.requirements.txt
for tool in cmake ctest cpack gcovr; do
  $SUDO ln -sf "/opt/coverage-tools/bin/$tool" "/usr/local/bin/$tool"
done
# <<< CMake and gcovr

# >>> LLVM
# The distribution's LLVM must not be installed beside it: CMake could find
# its Clang or Polly packages, and the JIT would link two LLVMs.
llvm_prefix=/usr/lib/llvm-${LLVM_VERSION}
others=$(dpkg-query -W -f='${db:Status-Abbrev} ${Package}\n' \
           'llvm-*-dev' 'libclang-*-dev' 'libpolly-*-dev' 2>/dev/null \
         | awk -v v="$LLVM_VERSION" '$1 ~ /^ii/ && $2 !~ ("-" v "-dev$") { print $2 }' | tr '\n' ' ')
if [ -n "$others" ]; then
  echo "::error::LLVM development packages of another version are installed: ${others}"
  exit 1
fi
cmake_dirs="-DLLVM_DIR=${llvm_prefix}/lib/cmake/llvm -DClang_DIR=${llvm_prefix}/lib/cmake/clang"
if [ -d "${llvm_prefix}/lib/cmake/polly" ]; then
  cmake_dirs="$cmake_dirs -DPolly_DIR=${llvm_prefix}/lib/cmake/polly"
fi
if [ -n "${GITHUB_ENV:-}" ]; then
  {
    echo "LLVM_VERSION=${LLVM_VERSION}"
    echo "CC=clang-${LLVM_VERSION}"
    echo "CXX=clang++-${LLVM_VERSION}"
    echo "GCOV=llvm-cov-${LLVM_VERSION} gcov"
    echo "CMAKEFLAGS=${CMAKEFLAGS:+$CMAKEFLAGS }${cmake_dirs}"
  } >> "$GITHUB_ENV"
fi

# apt.llvm.org's packages are not pinned: what this run built and measured with
# is printed here.
apt-cache policy "clang-${LLVM_VERSION}" "llvm-${LLVM_VERSION}-dev" \
  "libclang-${LLVM_VERSION}-dev" "libclang-rt-${LLVM_VERSION}-dev" "lld-${LLVM_VERSION}"
"clang++-${LLVM_VERSION}" --version
"llvm-config-${LLVM_VERSION}" --version
# <<< LLVM

cmake --version
gcovr --version

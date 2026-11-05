#!/bin/bash -eux
# Dependencies of the coverage job (.github/workflows/coverage.yml), which runs
# in an ubuntu:26.10 container.
#
# - LLVM / clang from apt.llvm.org: the JIT plug-in builds against it, so the
#   process sweeps also run the JIT, Avendish and C++ texture generator
#   processes. apt.llvm.org has no 26.10 repository; its 26.04 one installs
#   cleanly on 26.10, which only carries newer versions of the libraries it
#   links to.
# - Qt, Mesa and the rest from the distribution, through ci/ubuntu.2604.deps.sh,
#   whose package names 26.10 shares.
# - CMake and gcovr from PyPI, where their releases land first.

source ci/common.setup.sh

LLVM_VERSION=23
LLVM_REPO=llvm-toolchain-resolute-${LLVM_VERSION}
LLVM_KEY_FINGERPRINT=6084F3CF814B57C1CF12EFD515CF4D18AF4F7421
CMAKE_VERSION=4.4.4
GCOVR_VERSION=8.8

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
$SUDO tee /etc/apt/sources.list.d/apt.llvm.org.sources >/dev/null <<EOF
Types: deb
URIs: https://apt.llvm.org/resolute/
Suites: ${LLVM_REPO}
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
# plug-in update cannot change what the sweeps test without a commit here.
$SUDO apt-get install -y --no-install-recommends \
  dpf-plugins-clap=1.7+ds-3build2 dpf-plugins-lv2=1.7+ds-3build2 \
  dpf-plugins-vst=1.7+ds-3build2 dpf-plugins-vst3=1.7+ds-3build2 \
  lv2-examples=1.18.10-3
# <<< validation audio plug-ins

# >>> CMake and gcovr
# /usr/local/bin comes before /usr/bin, where ubuntu.2604.deps.sh puts the
# distribution's CMake.
$SUDO python3 -m venv /opt/coverage-tools
$SUDO /opt/coverage-tools/bin/pip install --no-cache-dir \
  "cmake==${CMAKE_VERSION}" "gcovr==${GCOVR_VERSION}"
for tool in cmake ctest cpack gcovr; do
  $SUDO ln -sf "/opt/coverage-tools/bin/$tool" "/usr/local/bin/$tool"
done
# <<< CMake and gcovr

cmake --version
gcovr --version
"clang++-${LLVM_VERSION}" --version

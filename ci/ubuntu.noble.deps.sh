#!/bin/bash -eux

source ci/common.setup.sh

echo 'debconf debconf/frontend select Noninteractive' | $SUDO debconf-set-selections

# For newer CMake
$SUDO apt-get install -y ca-certificates gpg wget lsb-release software-properties-common
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | $SUDO tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main' | $SUDO tee /etc/apt/sources.list.d/kitware.list >/dev/null
$SUDO apt-get update -qq
$SUDO rm /usr/share/keyrings/kitware-archive-keyring.gpg
$SUDO apt-get install -y kitware-archive-keyring

# LLVM 23 is provided by apt.llvm.org rather than Noble's archive.
LLVM_KEY_FINGERPRINT=6084F3CF814B57C1CF12EFD515CF4D18AF4F7421
keyring=/etc/apt/keyrings/apt.llvm.org.asc
$SUDO install -d -m 0755 /etc/apt/keyrings
wget -qO - https://apt.llvm.org/llvm-snapshot.gpg.key | $SUDO tee "$keyring" >/dev/null
if ! gpg --show-keys --with-colons "$keyring" | grep -q "^fpr:::::::::${LLVM_KEY_FINGERPRINT}:"; then
  echo "apt.llvm.org key does not have the fingerprint ${LLVM_KEY_FINGERPRINT}" >&2
  exit 1
fi
$SUDO tee /etc/apt/sources.list.d/apt.llvm.org.sources >/dev/null <<EOF
Types: deb
URIs: https://apt.llvm.org/noble/
Suites: llvm-toolchain-noble-23
Components: main
Signed-By: ${keyring}
EOF

$SUDO apt-get update -qq
$SUDO apt-get install -y \
    --allow-change-held-packages \
    --allow-downgrades \
    --allow-remove-essential \
    --allow-unauthenticated \
     binutils gcc-14 g++-14 clang-23 lld-23 \
     software-properties-common wget \
     libasound-dev \
     ninja-build cmake \
     libfftw3-dev \
     libdbus-1-dev \
     libsuil-dev liblilv-dev lv2-dev \
     libx11-dev libxext-dev libxcomposite-dev libxrandr-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev qt6-svg-dev qt6-connectivity-dev \
     libqt6opengl6-dev \
     libqt6websockets6-dev \
     libqt6serialport6-dev \
     libqt6shadertools6-dev \
     libbluetooth-dev libsdl2-dev libsdl2-2.0-0 \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libpipewire-0.3-dev \
     libclang-23-dev llvm-23-dev libclang-rt-23-dev \
     libvulkan-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
     file \
     dpkg-dev \
     lsb-release

# Both GCC and CMake's multiarch detection need the unversioned linker name.
$SUDO rm -rf /usr/bin/lld /usr/bin/ld.lld

$SUDO ln -s /usr/bin/lld-23 /usr/bin/lld
$SUDO ln -s /usr/bin/ld.lld-23 /usr/bin/ld.lld

source ci/common.deps.sh LINUX

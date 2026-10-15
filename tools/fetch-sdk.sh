#!/bin/bash

if [[ $# > 0 ]]; then
  export SDK_VERSION=$1
else
  export SDK_VERSION=sdk44
fi

echo "Running on OSTYPE: '$OSTYPE'"
echo "Using SDK: $SDK_VERSION"
export LATEST_TAG=$(git describe --tags --abbrev=0)
export LATEST_TAG_NOV=$(echo "$LATEST_TAG" | sed "s/v//")
export BASE_SDK=https://github.com/ossia/score-sdk/releases/download/$SDK_VERSION
export BOOST_SDK=https://github.com/ossia/score-sdk/releases/download/sdk31
export BOOST_VER=boost_1_90_0
export LATEST_RELEASE=https://github.com/ossia/score/releases/download/$LATEST_TAG
export CMAKE_VERSION=4.3.2

# A download that fails stops here, with its URL and reason. Otherwise a dropped
# connection or an HTTP error page left a missing or truncated archive behind and
# the script reported whatever broke next -- a failed 7-Zip download surfaced as
# "SDK error: clang not found".
fetch() {
  local url=$1 out=$2
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 --retry-all-errors --retry-delay 5 -o "$out" "$url" && return 0
  else
    wget -nv --tries=3 -O "$out" "$url" && return 0
  fi
  echo "fetch-sdk: could not download $url" >&2
  rm -f "$out"
  return 1
}

if [[ "$OSTYPE" == "darwin"* ]]; then
(
  # First download the compiler and base libraries
  if [[ "$(uname -m)" == "arm64" ]]; then
    SDK_ARCH=aarch64
  else
    SDK_ARCH=x86_64
  fi

  SDK_DIR=/opt/ossia-sdk-$SDK_ARCH
  sudo mkdir -p "$SDK_DIR"
  sudo chown -R $(whoami) /opt
  sudo chmod -R a+rwx /opt

  (
    SDK_ARCHIVE=sdk-macOS-$SDK_ARCH.tar.xz
    fetch "$BASE_SDK/$SDK_ARCHIVE" "$SDK_ARCHIVE" || exit 1
    tar -xzf "$SDK_ARCHIVE" --strip-components=2 --directory "$SDK_DIR" || exit 1
    rm -rf "$SDK_ARCHIVE"
  ) || exit 1

  # Download boost
  (
    BOOST="$BOOST_VER.tar.gz"
    fetch "$BOOST_SDK/$BOOST" "$BOOST" || exit 1
    mkdir -p "$SDK_DIR/boost/include"
    tar -xzf "$BOOST" --strip-components=1 --directory "$SDK_DIR/boost/include" || exit 1
  ) || exit 1

  ls "$SDK_DIR"
)
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
(
  # First download the compiler and base libraries
  SDK_ARCH="$(uname -m)"
  SDK_DIR=/opt/ossia-sdk-$SDK_ARCH
  SUDO=$(command -v sudo 2>/dev/null || true)
  $SUDO mkdir -p "$SDK_DIR"
  $SUDO chown -R $(whoami) "$SDK_DIR"

  (
    SDK_ARCHIVE=sdk-linux-$SDK_ARCH.tar.xz
    fetch "$BASE_SDK/$SDK_ARCHIVE" "$SDK_ARCHIVE" || exit 1
    tar xaf "$SDK_ARCHIVE" --strip-components=2 --directory "$SDK_DIR" || exit 1
    rm -rf "$SDK_ARCHIVE"
  ) || exit 1
  ls "$SDK_DIR"/

  # Download boost
  (
    BOOST="$BOOST_VER.tar.gz"
    fetch "$BOOST_SDK/$BOOST" "$BOOST" || exit 1
    mkdir -p "$SDK_DIR/boost/include"
    tar -xzf "$BOOST" --strip-components=1 --directory "$SDK_DIR/boost/include" || exit 1
  ) || exit 1

  ls "$SDK_DIR"
)
else
(
  # First download the compiler and base libraries
  SDK_DIR=/c/ossia-sdk-x86_64
  mkdir "$SDK_DIR"
  cd "$SDK_DIR"

  if [[ ! -f "$SDK_DIR/llvm/bin/clang.exe" ]] ; then
  (
    SDK_ARCHIVE=sdk-mingw-x86_64.7z
    echo "<< downloading sdk >>"
    fetch "$BASE_SDK/$SDK_ARCHIVE" "$SDK_ARCHIVE" || exit 1

    echo "<< downloading 7z >>"
    fetch "https://www.7-zip.org/a/7zr.exe" 7z.exe || exit 1

    echo "<< extracting sdk >>"
    ./7z.exe x sdk-mingw-x86_64.7z || exit 1

    rm "sdk-mingw-x86_64.7z" "7z.exe"
  ) || exit 1
  fi
  ls "$SDK_DIR"
  
  if [[ ! -f "$SDK_DIR/llvm/bin/clang.exe" ]] ; then
    echo "SDK error: clang not found"
    exit 1
  fi

  # Download cmake
  if [[ -d "$SDK_DIR/cmake" ]] ; then
    rm -rf "$SDK_DIR/cmake"
  fi

  echo "CMake not found, installing..."
  fetch "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-windows-x86_64.zip" cmake.zip || exit 1
  unzip -qq cmake.zip || exit 1
  mv cmake-$CMAKE_VERSION-windows-x86_64 cmake

  if [[ -d "$SDK_DIR/cmake" ]] ; then
    export PATH="$PATH:$SDK_DIR/cmake/bin"
  fi

  # Download boost
  if [[ ! -d "$SDK_DIR/boost" ]] ; then
  (
    echo "<< downloading Boost >>"
    BOOST="$BOOST_VER.tar.gz"
    fetch "$BOOST_SDK/$BOOST" "$BOOST" || exit 1
    mkdir -p "$SDK_DIR/boost/include"

    echo "<< extracting Boost: >> $BOOST => $SDK_DIR/boost/include/"
    tar xaf "$BOOST" || exit 1
    mv boost_*/boost "$SDK_DIR/boost/include/"
    rm *.gz
    rm -rf $BOOST

    ls  "$SDK_DIR/boost/include/"
  ) || exit 1
  fi


  # Download ninja
  if [[ ! -f "$SDK_DIR/llvm/bin/ninja.exe" ]] ; then
    if ! command -v ninja >/dev/null 2>&1 ; then
    (
      fetch https://github.com/ninja-build/ninja/releases/download/v1.13.1/ninja-win.zip ninja-win.zip || exit 1
      tar xaf ninja-win.zip || exit 1
      mv ninja.exe "$SDK_DIR/llvm/bin/"
      rm ninja-win.zip
    ) || exit 1
    fi
  fi
)
fi

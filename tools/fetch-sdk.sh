#!/bin/bash

if [[ $# > 0 ]]; then
  export SDK_VERSION=$1
else
  export SDK_VERSION=sdk34
fi

echo "Running on OSTYPE: '$OSTYPE'"
echo "Using SDK: $SDK_VERSION"
export LATEST_TAG=$(git describe --tags --abbrev=0)
export LATEST_TAG_NOV=$(echo "$LATEST_TAG" | sed "s/v//")
export BASE_SDK=https://github.com/ossia/score-sdk/releases/download/$SDK_VERSION
export BOOST_SDK=https://github.com/ossia/score-sdk/releases/download/sdk31
export BOOST_VER=boost_1_88_0
export LATEST_RELEASE=https://github.com/ossia/score/releases/download/$LATEST_TAG
export CMAKE_VERSION=4.1.2

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
    wget -nv "$BASE_SDK/$SDK_ARCHIVE" -O "$SDK_ARCHIVE"
    tar -xzf "$SDK_ARCHIVE" --strip-components=2 --directory "$SDK_DIR"
    rm -rf "$SDK_ARCHIVE"
  )

  # Download boost
  (
    BOOST="$BOOST_VER.tar.gz"
    wget -nv "$BOOST_SDK/$BOOST" -O "$BOOST"
    mkdir -p "$SDK_DIR/boost/include"
    tar -xzf "$BOOST" --strip-components=1 --directory "$SDK_DIR/boost/include"
  )

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
    wget -nv "$BASE_SDK/$SDK_ARCHIVE" -O "$SDK_ARCHIVE"
    tar xaf "$SDK_ARCHIVE" --strip-components=2 --directory "$SDK_DIR"
    rm -rf "$SDK_ARCHIVE"
  )
  ls "$SDK_DIR"/

  # Download boost
  (
    BOOST="$BOOST_VER.tar.gz"
    wget -nv "$BOOST_SDK/$BOOST" -O "$BOOST"
    mkdir -p "$SDK_DIR/boost/include"
    tar -xzf "$BOOST" --strip-components=1 --directory "$SDK_DIR/boost/include"
  )

  ls "$SDK_DIR"
)
else
(
  # First download the compiler and base libraries
  SDK_DIR=/c/ossia-sdk-x86_64
  mkdir "$SDK_DIR"
  cd "$SDK_DIR"

  # Download cmake
  if [[ ! -d "$SDK_DIR/cmake" ]] ; then
    if ! command -v cmake >/dev/null 2>&1 ; then
      echo "CMake not found, installing..."
      curl -L -0 https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-windows-x86_64.zip --output cmake.zip
      unzip -qq cmake.zip
      mv cmake-$CMAKE_VERSION-windows-x86_64 cmake
    fi
  fi

  if [[ -d "$SDK_DIR/cmake" ]] ; then
    export PATH="$PATH:$SDK_DIR/cmake/bin"
  fi

  if [[ ! -f "$SDK_DIR/llvm/bin/clang.exe" ]] ; then
  (
    SDK_ARCHIVE=sdk-mingw-x86_64.7z
    echo "<< downloading sdk >>"
    curl -L -O "$BASE_SDK/$SDK_ARCHIVE"

    echo "<< downloading 7z >>"
    curl -L "https://www.7-zip.org/a/7zr.exe" -o 7z.exe
    
    echo "<< extracting sdk >>"
    ./7z.exe x sdk-mingw-x86_64.7z
    
    rm "sdk-mingw-x86_64.7z" "7z.exe"
  )
  fi
  ls "$SDK_DIR"
  
  if [[ ! -f "$SDK_DIR/llvm/bin/clang.exe" ]] ; then
    echo "SDK error: clang not found"
    exit 1
  fi

  # Download boost
  if [[ ! -d "$SDK_DIR/boost" ]] ; then
  (
    echo "<< downloading Boost >>"
    BOOST="$BOOST_VER.tar.gz"
    curl -L -0 "$BOOST_SDK/$BOOST" --output "$BOOST"
    mkdir -p "$SDK_DIR/boost/include"

    echo "<< extracting Boost: >> $BOOST => $SDK_DIR/boost/include/"
    tar xaf "$BOOST"
    mv boost_*/boost "$SDK_DIR/boost/include/"
    rm *.gz
    rm -rf $BOOST

    ls  "$SDK_DIR/boost/include/"
  )
  fi


  # Download ninja
  if [[ ! -f "$SDK_DIR/llvm/bin/ninja.exe" ]] ; then
    if ! command -v ninja >/dev/null 2>&1 ; then
    (
      curl -L -0 https://github.com/ninja-build/ninja/releases/download/v1.13.1/ninja-win.zip --output ninja-win.zip
      tar xaf ninja-win.zip
      mv ninja.exe "$SDK_DIR/llvm/bin/"
      rm ninja-win.zip
    )
    fi
  fi
)
fi

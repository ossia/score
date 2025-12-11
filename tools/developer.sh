#!/bin/bash -eux
echo "Running on OSTYPE: '$OSTYPE'"
DISTRO=""
CODENAME=""
export SUDO=$(command -v sudo 2>/dev/null)
command -v git >/dev/null 2>&1 || { echo >&2 "Please install git."; exit 1; }

if [[ -d ../.git ]]; then
  cd ..
elif [[ -d score/.git ]]; then
  cd score
elif [[ ! -d .git ]]; then
  git clone --recursive -j16 https://github.com/ossia/score
  cd score
fi
SCORE_PATH=$PWD
if [[ "$#" -eq 0 ]];
  then
  BUILD_DIR=build-developer
elif [[ "$# " -eq 1 ]]; then
  BUILD_DIR=$1
fi

detect_deps_script() {
    case "$CODENAME" in
      bookworm)
        DEPS=debian.bookworm
        return 0;;
      trixie)
        DEPS=debian.trixie
        return 0;;
      sid)
        DEPS=debian.trixie
        return 0;;
      jammy)
        DEPS=ubuntu.jammy
        return 0;;
      noble)
        DEPS=ubuntu.noble
        return 0;;
      lunar)
        DEPS=ubuntu.lunar
        return 0;;
      oracular)
        DEPS=ubuntu.oracular
        return 0;;
      plucky)
        DEPS=ubuntu.plucky
        return 0;;
      leap)
        DEPS=suse-leap
        return 0;;
      tumbleweed)
        DEPS=suse-tumbleweed
        return 0;;
    esac

    case "$DISTRO" in
      arch)
        DEPS=archlinux
        QT=6
        return 0;;
      debian)
        DEPS=debian.bookworm
        QT=6
        return 0;;
      ubuntu)
        DEPS=ubuntu.noble
        return 0;;
      fedora)
        DEPS=fedora
        return 0;;
      centos)
        DEPS=fedora
        return 0;;
      suse)
        DEPS=suse-leap
        return 0;;
    esac

    echo "[developer.sh] Could not detect the build script to use."
    return 1
}

detect_linux_distro() {
    DISTRO=$(awk -F'=' '/^ID=/ {gsub("\"","",$2); print tolower($2) } ' /etc/*-release 2> /dev/null)
    CODENAME=""
    QT=6
    case "$DISTRO" in
      arch)
        return 0;;
      debian)
        CODENAME=$(cat /etc/*-release  | grep CODENAME | head -n 1 | cut -d= -f2)
        if [[ "$CODENAME" == "" ]]; then
          CODENAME=sid
        fi
        return 0;;
      ubuntu)
        CODENAME=$(cat /etc/*-release | grep CODENAME | head -n 1 | cut -d= -f2)
        return 0;;
      fedora)
        return 0;;
      centos)
        return 0;;
      opensuse-leap)
        DISTRO=suse
        CODENAME=leap
        return 0;;
      opensuse-tumbleweed)
        DISTRO=suse
        CODENAME=tumbleweed
        return 0;;
      *suse*)
        DISTRO=suse
        CODENAME=leap
        return 0;;
    esac

    if command -v pacman >/dev/null 2>&1; then
      DISTRO=arch
      return 0
    fi
 
    if command -v dnf >/dev/null 2>&1; then
      DISTRO=fedora
      return 0
    fi

    if command -v yum >/dev/null 2>&1; then
      DISTRO=centos
      return 0
    fi

    if command -v zypper >/dev/null 2>&1; then
      DISTRO=suse
      CODENAME=leap
      return 0
    fi

    if command -v apt >/dev/null 2>&1; then
      DISTRO=debian
      CODENAME=$(cat /etc/*-release | grep CODENAME | head -n 1 | cut -d= -f2)
      if [[ "$CODENAME" == "" ]]; then
        CODENAME=sid
      fi
      return 0
    fi

    echo "[developer.sh] Could not detect the Linux distribution."
    return 1
}

if [[ "$OSTYPE" == "darwin"* ]]; then
(
  command -v brew >/dev/null 2>&1 || { echo >&2 "Please install Homebrew."; exit 1; }
  (brew list | grep qt@5 >/dev/null) && { echo >&2 "Please remove qt@5 as it is incompatible with the required Homebrew Qt 6 package"; exit 1; }
  
  echo "[developer.sh] Installing dependencies"
  BREW_PACKAGES=(cmake qt boost ffmpeg fftw portaudio sdl lv2 lilv freetype)
  brew update
  brew upgrade
  brew install ninja ${BREW_PACKAGES[@]}
  brew cleanup
  
  echo "[developer.sh] Configuring"
  mkdir -p $BUILD_DIR
  cd $BUILD_DIR
  
  # Generate CMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix ffmpeg);..."
  CMAKE_PREFIX_PATH=
  for pkg in "${BREW_PACKAGES[@]}"; do
   CMAKE_PREFIX_PATH+="$(brew --prefix ${pkg} -q);"
  done
  CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH::${#CMAKE_PREFIX_PATH}-1}
  echo "$CMAKE_PREFIX_PATH"

  if [[ ! -f ./score ]]; then
    cmake -Wno-dev \
        $SCORE_PATH \
        -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DSCORE_PCH=1 \
        -DSCORE_DYNAMIC_PLUGINS=1 \
        -DCMAKE_COLOR_DIAGNOSTICS=1
  fi
  
  echo "[developer.sh] Building in $PWD/build-developer"
  cmake --build .

  if [[ -f ossia-score ]]; then
    echo "[developer.sh] Build successful, you can run $PWD/score."
    echo "[developer.sh] To rebuild, run: 'cd $PWD; cmake --build .'"
  fi
)
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
(
  echo "[developer.sh] Installing dependencies"
  detect_linux_distro
  detect_deps_script
  "ci/$DEPS.deps.sh"

  if command -v clang++-22 ; then
    CC=clang-22
    CXX=clang++-22
  elif command -v clang++-21 ; then
    CC=clang-21
    CXX=clang++-21
  elif command -v clang++-20 ; then
    CC=clang-20
    CXX=clang++-20
  elif command -v clang++-19 ; then
    CC=clang-19
    CXX=clang++-19
  elif command -v clang++-18 ; then
    CC=clang-18
    CXX=clang++-18
  elif command -v clang++ ; then
    CC=clang
    CXX=clang++
  else
    CC=/usr/bin/cc
    CXX=/usr/bin/c++
  fi

  if command -v ld.mold ; then
    LFLAG='-fuse-ld=mold'
  elif command -v ld.lld ; then
    LFLAG='-fuse-ld=lld'
  else
    LFLAG=''
  fi

  echo "[developer.sh] Configuring"
  mkdir -p $BUILD_DIR
  cd $BUILD_DIR

  if [[ ! -f ./ossia-score ]]; then
    cmake -Wno-dev \
        $SCORE_PATH \
        -GNinja \
        -DCMAKE_C_COMPILER=$CC \
        -DCMAKE_CXX_COMPILER=$CXX \
        -DCMAKE_EXE_LINKER_FLAGS="$LFLAG" \
        -DCMAKE_SHARED_LINKER_FLAGS="$LFLAG" \
        -DCMAKE_MODULE_LINKER_FLAGS="$LFLAG" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DSCORE_PCH=1 \
        -DSCORE_DYNAMIC_PLUGINS=1 \
        -DCMAKE_COLOR_DIAGNOSTICS=1
  fi
  
  echo "[developer.sh] Building in $PWD"
  cmake --build .

  if [[ -f ossia-score ]]; then
    echo "[developer.sh] Build successful, you can run $PWD/ossia-score"
    echo "[developer.sh] To rebuild, run: 'cd $PWD; cmake --build .'"
  fi
)
else
(
  echo "[developer.sh] Installing dependencies"
  if command -v pacman; then
    pacman -Syyu
    if ! command -v pacboy; then
      pacman -S pactoys --noconfirm
    fi
    pacboy -S --needed \
      cmake:p \
      ninja:p \
      toolchain:p \
      cppwinrt:p \
      qt6-base:p \
      qt6-declarative:p \
      qt6-websockets:p \
      qt6-serialport:p \
      qt6-shadertools:p \
      qt6-scxml:p \
      qt6-tools:p \
      boost:p \
      portaudio:p \
      fftw:p \
      ffmpeg:p \
      SDL2:p
      
    echo "[developer.sh] Configuring ($MINGW_PREFIX)"
    mkdir -p $BUILD_DIR
    cd $BUILD_DIR
    if [[ ! -f ./ossia-score ]]; then
      cmake -Wno-dev \
          $SCORE_PATH \
          -GNinja \
          -DCMAKE_BUILD_TYPE=Debug \
          -DSCORE_PCH=1 \
          -DCMAKE_COLOR_DIAGNOSTICS=1 \
          -DCMAKE_OPTIMIZE_DEPENDENCIES=1 \
          -DCMAKE_LINK_DEPENDS_NO_SHARED=1 \
          -DSCORE_DYNAMIC_PLUGINS=0
    fi

    echo "[developer.sh] Building in $PWD"
    cmake --build .
    
    if [[ -f ossia-score ]]; then
      echo "[developer.sh] Build successful, you can run $PWD/ossia-score"
      echo "[developer.sh] To rebuild, run: 'cd $PWD; cmake --build .'"
    fi    
  else
    source tools/fetch-sdk.sh
    
    PATH="$PATH:/c/ossia-sdk-x86_64/llvm/bin"
    if [[ -d /c/ossia-sdk-x86_64/cmake/bin ]]; then
      PATH="$PATH:/c/ossia-sdk-x86_64/cmake/bin"
    fi 
    
    echo "[developer.sh] Configuring (SDK)"
    mkdir -p $BUILD_DIR
    cd $BUILD_DIR
    
    cp -f /c/ossia-sdk-x86_64/llvm/bin/libunwind.dll .
    cp -f /c/ossia-sdk-x86_64/llvm/bin/libc++.dll .
    cp -f /c/ossia-sdk-x86_64/llvm/x86_64-w64-mingw32/bin/libwinpthread-1.dll .
    if [[ ! -f ./ossia-score ]]; then
      echo "[developer.sh] Configuring in $PWD"
      cmake -Wno-dev \
          $SCORE_PATH \
          -GNinja \
          -DOSSIA_SDK=c:/ossia-sdk-x86_64 \
          -DCMAKE_CXX_FLAGS="-fexperimental-library" \
          -DCMAKE_C_COMPILER=c:/ossia-sdk-x86_64/llvm/bin/clang.exe \
          -DCMAKE_CXX_COMPILER=c:/ossia-sdk-x86_64/llvm/bin/clang++.exe \
          -DCMAKE_BUILD_TYPE=Debug \
          -DSCORE_PCH=1 \
          -DCMAKE_COLOR_DIAGNOSTICS=1 \
          -DCMAKE_OPTIMIZE_DEPENDENCIES=1 \
          -DCMAKE_LINK_DEPENDS_NO_SHARED=1 \
          -DSCORE_DYNAMIC_PLUGINS=0
    fi
    
    echo "[developer.sh] Building in $PWD"
    cmake --build .
    
    if [[ -f ossia-score ]]; then
      echo "[developer.sh] Build successful, you can run $PWD/ossia-score"
      echo "[developer.sh] To rebuild, run: 'cd $PWD; cmake --build .'"
    fi
  fi
)
fi

#!/bin/bash -eux

# In this case everything is built in docker
if [[ "$CONF" == "linux-package-appimage" ]];
then
    exit 0
fi
if [[ "$CONF" == "tarball" ]];
then
  exit 0
fi

# Install the deps
case "$TRAVIS_OS_NAME" in
  linux)
    sudo chmod -R a+rwx /opt

    sudo apt-get update -qq
    sudo apt-get install wget software-properties-common

    wget -nv https://github.com/jcelerier/cninja/releases/download/v3.7.4/cninja-v3.7.4-Linux.tar.gz -O cninja.tgz &
    wget -nv https://github.com/Kitware/CMake/releases/download/v3.18.2/cmake-3.18.2-Linux-x86_64.tar.gz -O cmake.tgz &
    echo 'deb http://apt.llvm.org/focal/ llvm-toolchain-focal-10 main' | sudo tee /etc/apt/sources.list.d/llvm.list
    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 1397BC53640DB551
    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 15CF4D18AF4F7421

    sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
    sudo add-apt-repository --yes ppa:beineri/opt-qt-5.15.0-focal

    sudo apt-get update -qq
    sudo apt-get install -qq --force-yes \
        g++-10 binutils libasound-dev ninja-build \
        gcovr lcov \
        qt515base qt515declarative qt515svg qt515quickcontrols2 qt515websockets qt515serialport \
        libgl1-mesa-dev \
        libavcodec-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
        portaudio19-dev \
        clang-10 lld-10 libc++-10-dev libc++abi-10-dev \
        libbluetooth-dev \
        libsdl2-dev libsdl2-2.0-0 libglu1-mesa-dev libglu1-mesa \
        libgles2-mesa-dev \
        libavahi-compat-libdnssd-dev libsamplerate0-dev

    wait || true

    tar xaf cninja.tgz
    sudo cp -rf cninja /usr/bin/
    tar xaf cmake.tgz
    mv cmake-*-x86_64 cmake-latest
  ;;
  osx)
#    echo "=============================="
#    find /Applications/Xcode.app -name MacOSX10.15.sdk -type d
#    find /Applications/Xcode.app -iname CoreVideo.framework
#    ls /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/
#    ls /Applications/Xcode-12.2.beta.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/
#    ls /Applications/Xcode*.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/
#    ls /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/
#    ls /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/
#    ls /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/
#    ls /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/CoreVideo.framework
#    ls /Applications/Xcode-12.2.beta.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/
#    ls /Applications/Xcode-12.2.beta.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/
#    ls /Applications/Xcode-12.2.beta.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/
#    echo "=============================="

    

    # Setup codesigning
    # Thanks https://www.update.rocks/blog/osx-signing-with-travis/
    (
      set +x
      KEY_CHAIN=build.keychain

      openssl aes-256-cbc -K $encrypted_5c96fe262983_key -iv $encrypted_5c96fe262983_iv \
              -in /Users/travis/build/ossia/score/tools/travis/ossia-cert.p12.enc \
              -out ossia-cert.p12 -d

      security create-keychain -p travis $KEY_CHAIN
      security default-keychain -s $KEY_CHAIN
      security unlock-keychain -p travis $KEY_CHAIN
      security import ossia-cert.p12 -k $KEY_CHAIN -P $MAC_CODESIGN_PASSWORD -T /usr/bin/codesign;
      security set-key-partition-list -S apple-tool:,apple: -s -k travis $KEY_CHAIN

      rm -rf *.p12
    )

    set +e

    export HOMEBREW_NO_AUTO_UPDATE=1
    brew install gnu-tar ninja
    wget -nv https://github.com/jcelerier/cninja/releases/download/v3.7.4/cninja-v3.7.4-macOS.tar.gz -O cninja.tgz &
    
    SDK_ARCHIVE=score-sdk-mac.tar.gz
    wget -nv https://github.com/ossia/score-sdk/releases/download/sdk16/$SDK_ARCHIVE -O $SDK_ARCHIVE
    sudo mkdir -p /opt/score-sdk-osx
    sudo chmod -R a+rwx /opt/score-sdk-osx
    gtar xhaf $SDK_ARCHIVE --directory /opt/score-sdk-osx
    sudo rm -rf /Library/Developer/CommandLineTools
    sudo rm -rf /usr/local/include/c++

    wait || true
    gtar xhaf cninja.tgz
    sudo cp -rf cninja /usr/local/bin/

    set -e
  ;;
esac

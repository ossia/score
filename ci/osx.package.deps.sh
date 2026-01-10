#!/bin/bash -eux

# Setup codesigning
# Thanks https://www.update.rocks/blog/osx-signing-with-travis/

(
    set +x

    if [[ -n "${MAC_CERT_B64}" ]]; then
      echo "$MAC_CERT_B64" | base64 --decode > ossia-cert.p12
      export CODESIGN_SECUREFILEPATH=$PWD/ossia-cert.p12
    else
      echo "No certificate"
      exit 0
    fi

    KEY_CHAIN=build.keychain
    security create-keychain -p travis "$KEY_CHAIN"
    security default-keychain -s "$KEY_CHAIN"
    security unlock-keychain -p travis "$KEY_CHAIN"
    security import "$CODESIGN_SECUREFILEPATH" -k "$KEY_CHAIN" -P "$MAC_CODESIGN_PASSWORD" -T /usr/bin/codesign > /dev/null 2>&1

    security set-key-partition-list -S apple-tool:,apple: -s -k travis "$KEY_CHAIN" > /dev/null 2>&1
    rm -rf "$CODESIGN_SECUREFILEPATH"
)

set +e

export HOMEBREW_NO_AUTO_UPDATE=1
brew list
brew remove -f opusfile sox ffmpeg libsndfile flac opus libbluray libogg libvorbis libshout speex theora qt qt@5 qtkeychain
brew install gnu-tar ninja xz
wget -nv "https://github.com/jcelerier/cninja/releases/download/v3.7.9/cninja-v3.7.9-macOS-$MACOS_ARCH.tar.gz" -O cninja.tgz &

SDK_ARCHIVE=sdk-macOS-$MACOS_ARCH.tar.xz
wget -nv https://github.com/ossia/sdk/releases/download/sdk34/$SDK_ARCHIVE -O "$SDK_ARCHIVE"

sudo mkdir -p "/opt/ossia-sdk-$MACOS_ARCH/"
sudo chown -R "$(whoami)" /opt
sudo chmod -R a+rwx /opt
gtar xhaf "$SDK_ARCHIVE" --strip-components=2 --directory "/opt/ossia-sdk-$MACOS_ARCH/"
ls "/opt/ossia-sdk-$MACOS_ARCH/"

sudo rm -rf /Library/Developer/CommandLineTools
sudo rm -rf /usr/local/include/c++

wait || true
gtar xhaf cninja.tgz
sudo cp -rf cninja /usr/local/bin/

set -e

source ci/common.deps.sh

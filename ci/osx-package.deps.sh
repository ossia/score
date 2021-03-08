#!/bin/bash -eux

# Setup codesigning
# Thanks https://www.update.rocks/blog/osx-signing-with-travis/
(
    set +x
    KEY_CHAIN=build.keychain

    security create-keychain -p travis $KEY_CHAIN
    security default-keychain -s $KEY_CHAIN
    security unlock-keychain -p travis $KEY_CHAIN

    security import $CODESIGN_SECUREFILEPATH -k $KEY_CHAIN -P $MAC_CODESIGN_PASSWORD -T /usr/bin/codesign > /dev/null 2>&1
    security set-key-partition-list -S apple-tool:,apple: -s -k travis $KEY_CHAIN > /dev/null 2>&1

    rm -rf $CODESIGN_SECUREFILEPATH
)

set +e

export HOMEBREW_NO_AUTO_UPDATE=1
brew install gnu-tar ninja
wget -nv https://github.com/jcelerier/cninja/releases/download/v3.7.5/cninja-v3.7.5-macOS.tar.gz -O cninja.tgz &
wget -nv https://github.com/ossia/sdk/releases/download/sdk18/MacOSX11.0.sdk.tar.gz &

SDK_ARCHIVE=sdk-macOS.tar.gz
wget -nv https://github.com/ossia/score-sdk/releases/download/sdk19/$SDK_ARCHIVE -O $SDK_ARCHIVE
sudo mkdir -p /opt/ossia-sdk
sudo chown -R $(whoami) /opt
sudo chmod -R a+rwx /opt
gtar xhaf $SDK_ARCHIVE --strip-components=2 --directory /opt/ossia-sdk/
ls /opt/ossia-sdk

sudo rm -rf /Library/Developer/CommandLineTools
sudo rm -rf /usr/local/include/c++

wait || true
gtar xhaf cninja.tgz
sudo cp -rf cninja /usr/local/bin/

echo "Copying sdks..."
gtar xhaf MacOSX11.0.sdk.tar.gz
mv MacOSX11.0.sdk /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/
sudo cp -rf cninja /usr/local/bin/
set -e

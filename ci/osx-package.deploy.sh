#!/bin/bash -eux
export TAG=$GITTAGNOV

export HOMEBREW_NO_AUTO_UPDATE=1
export SRC_PATH=$PWD
brew install graphicsmagick imagemagick create-dmg

cd $SRC_PATH/install/

# Codesign
sign_app() {
  codesign \
      --entitlements "$1" \
      --force \
      --timestamp \
      --options=runtime \
      --sign "ossia.io" \
      "$2"
}

echo " === code signing === "
security unlock-keychain -p travis build.keychain

sign_app "$SRC_PATH/src/vstpuppet/entitlements.plist" "score.app/Contents/MacOS/ossia-score-vstpuppet.app"
sign_app "$SRC_PATH/src/vst3puppet/entitlements.plist" "score.app/Contents/MacOS/ossia-score-vst3puppet.app"
sign_app "$SRC_PATH/src/app/entitlements.plist" "score.app"

echo " === create dmg === "
# Create a .dmg
cp $SRC_PATH/LICENSE.txt license.txt
security unlock-keychain -p travis build.keychain
create-dmg \
  --volname "ossia score $TAG" \
  --window-pos 200 120 \
  --window-size 800 400 \
  --icon-size 100 \
  --app-drop-link 600 185 \
  --icon score.app 200 190 \
  --hide-extension score.app \
  'score.dmg' 'score.app'
ls

echo " === notarize === "
# Notarize the .dmg
security unlock-keychain -p travis build.keychain
xcrun notarytool submit *.dmg \
  --team-id "GRW9MHZ724" \
  --apple-id "jeanmichael.celerier@gmail.com" \
  --password "@env:MAC_ALTOOL_PASSWORD" \
  --progress \
  --wait

xcrun stapler staple *.dmg

mv *.dmg "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$TAG-macOS.dmg"
mv "mac-sdk.zip" "$BUILD_ARTIFACTSTAGINGDIRECTORY/"

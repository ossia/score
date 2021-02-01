#!/bin/bash -eux
export TAG=$GITTAGNOV

export HOMEBREW_NO_AUTO_UPDATE=1
export SRC_PATH=$PWD
brew install graphicsmagick imagemagick npm
npm install --global create-dmg

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

security unlock-keychain -p travis build.keychain

sign_app "$SRC_PATH/src/vstpuppet/entitlements.plist" "score.app/Contents/MacOS/ossia-score-vstpuppet.app"
sign_app "$SRC_PATH/src/vst3puppet/entitlements.plist" "score.app/Contents/MacOS/ossia-score-vst3puppet.app"
sign_app "$SRC_PATH/src/app/entitlements.plist" "score.app"

# Create a .dmg
cp $SRC_PATH/LICENSE.txt license.txt
security unlock-keychain -p travis build.keychain
create-dmg 'score.app'
ls

# Notarize the .dmg
security unlock-keychain -p travis build.keychain
xcrun altool --notarize-app \
    -t osx \
    -f *.dmg \
    --primary-bundle-id "io.ossia.score" \
    -u jeanmichael.celerier@gmail.com \
    -p "@env:MAC_ALTOOL_PASSWORD" >altool.log

REQ_UUID=$(cat altool.log | grep Request | awk ' { print $3 } ')

# Wait until the notarization process completes to staple the result to the dmg file
sleep 30
for seconds in 30 30 30 30 30; do
    RES=$(xcrun altool --notarization-info $REQ_UUID -u jeanmichael.celerier@gmail.com -p "@env:MAC_ALTOOL_PASSWORD" --output-format xml)
    if [[ $(echo "$RES" | grep Approved) ]]; then
        xcrun stapler staple *.dmg
        echo "Stapling successful !"
        break
    elif [[ $(echo "$RES" | grep -i progress) ]]; then
        sleep $seconds
    else
        echo "Stapling failed ! Displaying log..."
        echo "$RES"
    fi
done

mv *.dmg "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$TAG-macOS.dmg"
mv "mac-sdk.zip" "$BUILD_ARTIFACTSTAGINGDIRECTORY/"

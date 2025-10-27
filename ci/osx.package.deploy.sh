#!/bin/bash -eux
export TAG=$GITTAGNOV
export HOMEBREW_NO_AUTO_UPDATE=1
export SRC_PATH="$PWD"

brew install graphicsmagick imagemagick create-dmg

cd "$SRC_PATH/install/"

# Codesign
sign_app() {
 local entitlements=${1}
 local folder=${2}

 if [[ -d "$folder" ]]; then
   find "$folder" -name '*.dylib' \
     -exec \
       codesign --force --timestamp --sign "ossia.io" {} \
     \;

    codesign \
        --entitlements "$entitlements" \
        --force \
        --timestamp \
        --options=runtime \
        --sign "ossia.io" \
        "$folder"
  else
    echo "'$folder' does not exist: skipping."
  fi
}

echo " === code signing === "
if [[ "${CI_IS_AZURE}" = "1" ]]; then
  echo "... unlock keychain "
  security unlock-keychain -p travis build.keychain
  export PACKAGE_ARCH=Intel
else
  if [[ "$MACOS_ARCH" = "x86_64" ]]; then
    export PACKAGE_ARCH=Intel
  else
    export PACKAGE_ARCH=AppleSilicon
  fi
fi

echo "... clappuppet "
sign_app "$SRC_PATH/src/clappuppet/entitlements.plist" "ossia score.app/Contents/MacOS/ossia-score-clappuppet.app"

echo "... vstpuppet "
sign_app "$SRC_PATH/src/vstpuppet/entitlements.plist" "ossia score.app/Contents/MacOS/ossia-score-vstpuppet.app"

echo "... vst3puppet "
sign_app "$SRC_PATH/src/vst3puppet/entitlements.plist" "ossia score.app/Contents/MacOS/ossia-score-vst3puppet.app"

echo "... score "
sign_app "$SRC_PATH/src/app/entitlements.plist" "ossia score.app"

echo " === create dmg === "
# Create a .dmg
cp "$SRC_PATH/LICENSE.txt" license.txt

if [[ "${CI_IS_AZURE}" = "1" ]]; then
  security unlock-keychain -p travis build.keychain
fi

echo killing...; sudo pkill -9 XProtect >/dev/null || true;
echo waiting...; while pgrep XProtect; do sleep 3; done;

max_tries=10
i=0
until sudo create-dmg \
  --volname "ossia score $TAG" \
  --window-pos 200 120 \
  --window-size 800 400 \
  --icon-size 100 \
  --app-drop-link 600 185 \
  --icon "ossia score.app" 200 190 \
  --hide-extension "ossia score.app" \
  --filesystem APFS \
  'score.dmg' 'ossia score.app'
do
  if [[ $i -eq $max_tries ]]; then
    echo 'Error: create-dmg did not succeed even after 10 tries.'
    exit 1
  fi
  ((i++))
done

echo
ls
echo " === set ownership === "

sudo chown "$(whoami)" ./score.dmg

# Notarize the .dmg
echo " === notarize === "
if [[ "${CI_IS_AZURE}" = "1" ]]; then
  security unlock-keychain -p travis build.keychain
fi

xcrun notarytool \
  submit score.dmg \
  --team-id "GRW9MHZ724" \
  --apple-id "jeanmichael.celerier@gmail.com" \
  --password "$MAC_ALTOOL_PASSWORD" \
  --progress \
  --wait

# Staple
xcrun stapler staple ./score.dmg
xcrun stapler validate ./score.dmg

[[ $? == 0 ]] || exit 1

# Archive
mv ./score.dmg "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$TAG-macOS-$PACKAGE_ARCH.dmg"
mv "mac-sdk.zip" "$BUILD_ARTIFACTSTAGINGDIRECTORY/sdk-darwin-$MACOS_ARCH.zip"

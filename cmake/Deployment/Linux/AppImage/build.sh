#!/bin/bash -eux
sudo rm -rf *AppDir*
sudo rm -rf *AppImage
#sudo rm -rf /tmp/build
#mkdir /tmp/build

: ${CPU_ARCH}

docker build --squash --compress --force-rm  --build-arg CPU_ARCH="${CPU_ARCH}" -f Dockerfile.llvm -t ossia/score-linux-llvm .

export SOURCE=$(git rev-parse --show-toplevel)
if [[ "${CPU_ARCH}" == "aarch64" ]]; then
  export OSSIA_SDK=/opt/ossia-sdk-aarch64
else
  export OSSIA_SDK=/opt/ossia-sdk
fi

docker run --rm -it \
-v "$(pwd)"/Recipe.llvm:/Recipe \
-e TOOLCHAIN=appimage-debug \
-e OSSIA_SDK="$OSSIA_SDK" \
-e CPU_ARCH="$CPU_ARCH" \
--mount type=bind,source="$SOURCE",target=/score \
--mount type=bind,source="/tmp/build",target=/build \
--mount type=bind,source="$OSSIA_SDK",target="$OSSIA_SDK" \
-w="/" \
ossia/score-linux-llvm \
/bin/bash Recipe


wget "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${CPU_ARCH}.AppImage" -O appimagetool-${CPU_ARCH}.AppImage
chmod a+x appimagetool-${CPU_ARCH}.AppImage
wget "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-${CPU_ARCH}" -O runtime-${CPU_ARCH}
chmod a+x runtime-${CPU_ARCH}
sudo chown -R "$(whoami)" /tmp/build
cp runtime-${CPU_ARCH} /tmp/build/score.AppDir/runtime-${CPU_ARCH}
cp ossia-score.desktop /tmp/build/score.AppDir/
cp "$SOURCE/src/lib/resources/ossia-score.png" /tmp/build/score.AppDir/
cp "$SOURCE/src/lib/resources/ossia-score.png" /tmp/build/score.AppDir/.DirIcon
(
  cd /tmp/build/score.AppDir
  rm -rf AppRun
  ln -s usr/bin/ossia-score AppRun
)
cp -rf /tmp/build/score.AppDir .

./appimagetool-${CPU_ARCH}.AppImage -n "/tmp/build/score.AppDir" "Score.AppImage" --runtime-file "runtime-${CPU_ARCH}"

#!/usr/bin/env bash

(
cd src/addons

clone_addon() {
  local url=${1}
  local folder=$(echo "${url}" | awk -F'/' '{print $NF}')

  (
  if [[ ! -d "$folder" ]]; then
    git clone --recursive -j16 "$url"
  else
    # Try to update the submodule if it's really super clean
    cd "$folder"
    git update-index --really-refresh
    if output=$(git status --porcelain  --untracked-files=no) && [ -z "$output" ]; then
      if output=$(git diff-index --quiet HEAD) && [ -z "$output" ]; then
        git pull || true
      fi
    fi
  fi
  )
}

clone_addon https://github.com/ossia/iscore-addon-network
clone_addon https://github.com/ossia/score-addon-synthimi
clone_addon https://github.com/ossia/score-addon-jk
clone_addon https://github.com/ossia/score-addon-puara

CI_PLATFORM="${1:-DEFAULT}"

if [[ "$CI_PLATFORM" != "WASM" ]];
then
  clone_addon https://github.com/ossia/score-addon-ltc
  clone_addon https://github.com/ossia/score-addon-ndi
  clone_addon https://github.com/bltzr/score-avnd-granola
  clone_addon https://github.com/ossia/score-addon-ultraleap
  clone_addon https://github.com/ossia/score-addon-contextfree
  clone_addon https://github.com/ossia/score-addon-ble
  clone_addon https://github.com/ossia/score-addon-led
  clone_addon https://github.com/ossia/score-addon-spatgris
  clone_addon https://github.com/ossia/score-addon-hdf5
  clone_addon https://github.com/ossia/score-addon-onnx
fi

cd score-addon-led
git checkout feature/gfx_led
)



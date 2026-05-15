#!/usr/bin/env bash

(
cd src/addons

clone_addon() {
  local url=${1}
  local ref=${2:-}
  local folder=$(echo "${url}" | awk -F'/' '{print $NF}')

  (
  if [[ ! -d "$folder" ]]; then
    git clone --recursive -j16 "$url"
    if [[ "x${ref}" != "x" ]]; then
    (
      cd "$folder"
      git checkout "${ref}"
    )
    fi
  else
    # Try to update the submodule if it's really super clean
    cd "$folder"
    git update-index --really-refresh
    if output=$(git status --porcelain  --untracked-files=no) && [ -z "$output" ]; then
      if output=$(git diff-index --quiet HEAD) && [ -z "$output" ]; then
        git pull || true
        if [[ "x${ref}" != "x" ]]; then
        (
          cd "$folder"
          git checkout "${ref}"
        )
        fi
      fi
    fi
  fi
  )
}

clone_addon https://github.com/ossia/iscore-addon-network
clone_addon https://github.com/ossia/score-addon-synthimi
clone_addon https://github.com/ossia/score-addon-jk
clone_addon https://github.com/ossia/score-addon-puara
clone_addon https://github.com/ossia/GBAP

CI_PLATFORM="${1:-DEFAULT}"

if [[ "$CI_PLATFORM" != "WASM" ]];
then
  clone_addon         https://github.com/ossia/score-addon-ltc
  clone_addon         https://github.com/ossia/score-addon-ndi feature/ndi_output_updates
  clone_addon      https://github.com/bltzr/score-avnd-granola
  clone_addon   https://github.com/ossia/score-addon-ultraleap
  clone_addon https://github.com/ossia/score-addon-contextfree
  clone_addon         https://github.com/ossia/score-addon-ble
  clone_addon         https://github.com/ossia/score-addon-led feature/gfx_led_2
  clone_addon    https://github.com/ossia/score-addon-spatgris
  clone_addon        https://github.com/ossia/score-addon-hdf5
  clone_addon        https://github.com/ossia/score-addon-onnx
  clone_addon   https://github.com/ossia/score-addon-deuterium
  clone_addon  https://github.com/ossia/score-addon-airwindows
  clone_addon         https://github.com/ossia/score-addon-lsl
  clone_addon             https://github.com/jcelerier/bendage
fi

if [[ "$CI_PLATFORM" == "LINUX" || "$CI_PLATFORM" == "WIN32" ]]; then
  clone_addon https://github.com/ossia/score-addon-librediffusion
fi
)



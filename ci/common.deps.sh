#!/usr/bin/env bash

(
cd src/addons

clone_addon() {
  local url=${1}
  local ref=${2:-}
  local folder=$(echo "${url}" | awk -F'/' '{print $NF}')

  # Shallow in CI (throwaway checkouts); full history locally so git blame works.
  local shallow=()
  if [[ -n "${GITHUB_ACTIONS:-}${CI:-}" && -z "${ref}" ]]; then
    shallow=(--depth 1 --shallow-submodules)
  fi

  (
  if [[ ! -d "$folder" ]]; then
    if [[ -n "${SKIP_SUBMODULE:-}" ]]; then
      # Skip a heavy nested submodule score never compiles (SKIP_SUBMODULE is
      # "<super-path> <submodule-name>"): clone, init that super, mark it none, recurse.
      local sdepth=(); [[ ${#shallow[@]} -gt 0 ]] && sdepth=(--depth 1)
      git clone "${sdepth[@]}" "$url" "$folder"
      (
        cd "$folder"
        git submodule update --init "${sdepth[@]}" "${SKIP_SUBMODULE%% *}"
        git -C "${SKIP_SUBMODULE%% *}" config "submodule.${SKIP_SUBMODULE#* }.update" none
        git submodule update --init --recursive "${sdepth[@]}"
      )
    else
      git clone --recursive -j16 "${shallow[@]}" "$url"
    fi
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
clone_addon https://github.com/ossia/GBAP
clone_addon https://github.com/ossia/score-addon-ltc
clone_addon https://github.com/bltzr/score-avnd-granola
clone_addon https://github.com/jcelerier/bendage
# libs/airwindows is ~600MB of regeneration-only sources; score compiles the
# committed src/autogen_airwin, so skip it.
SKIP_SUBMODULE="3rdparty/airwin2rack libs/autoexport_airwin/airwindows" \
  clone_addon https://github.com/ossia/score-addon-airwindows
clone_addon https://github.com/ossia/score-addon-cv
clone_addon https://github.com/ossia/score-addon-onnx
clone_addon https://github.com/ossia/score-addon-puara

CI_PLATFORM="${1:-DEFAULT}"

if [[ "$CI_PLATFORM" != "WASM" ]];
then
  clone_addon         https://github.com/ossia/score-addon-ble
  clone_addon https://github.com/ossia/score-addon-contextfree
  clone_addon   https://github.com/ossia/score-addon-deuterium
  clone_addon        https://github.com/ossia/score-addon-hdf5
  clone_addon         https://github.com/ossia/score-addon-led
  clone_addon         https://github.com/ossia/score-addon-lsl
  clone_addon         https://github.com/ossia/score-addon-ndi
  clone_addon    https://github.com/ossia/score-addon-spatgris
  clone_addon   https://github.com/ossia/score-addon-ultraleap
  clone_addon     https://github.com/ossia/score-addon-sysinfo
fi

if [[ "$CI_PLATFORM" == "LINUX" || "$CI_PLATFORM" == "WIN32" ]]; then
  clone_addon https://github.com/ossia/score-addon-librediffusion
fi
)



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
    if [[ -n "${NO_SUBMODULES:-}" ]]; then
      # An addon that vendors whole SDKs it only needs for its own standalone
      # build: score compiles the half that links into it, which is written not
      # to need them. Clone the top level and leave the submodules alone.
      local ndepth=(); [[ ${#shallow[@]} -gt 0 ]] && ndepth=(--depth 1)
      git clone ${ndepth[@]+"${ndepth[@]}"} "$url" "$folder"
    elif [[ -n "${SKIP_SUBMODULE:-}" ]]; then
      # Skip a heavy nested submodule score never compiles (SKIP_SUBMODULE is
      # "<super-path> <submodule-name>"): clone, init that super, mark it none, recurse.
      # ${arr[@]+...} guard: macOS bash 3.2 errors on empty arrays under set -u
      local sdepth=(); [[ ${#shallow[@]} -gt 0 ]] && sdepth=(--depth 1)
      git clone ${sdepth[@]+"${sdepth[@]}"} "$url" "$folder"
      (
        cd "$folder"
        git submodule update --init ${sdepth[@]+"${sdepth[@]}"} "${SKIP_SUBMODULE%% *}"
        git -C "${SKIP_SUBMODULE%% *}" config "submodule.${SKIP_SUBMODULE#* }.update" none
        git submodule update --init --recursive ${sdepth[@]+"${sdepth[@]}"}
      )
    else
      git clone --recursive -j16 ${shallow[@]+"${shallow[@]}"} "$url"
    fi
    if [[ "x${ref}" != "x" ]]; then
    (
      cd "$folder"
      git checkout "${ref}"
      # The ref may add or move submodules relative to the default branch
      # that was cloned recursively above: sync them or the addon builds
      # (or silently skips, see the note at the top) with the wrong trees.
      git submodule sync --recursive
      git submodule update --init --recursive
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
          git submodule sync --recursive
          git submodule update --init --recursive
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
  clone_addon      https://github.com/ossia/score-addon-openzen
  clone_addon    https://github.com/ossia/score-addon-spatgris
  clone_addon   https://github.com/ossia/score-addon-ultraleap
  clone_addon     https://github.com/ossia/score-addon-sysinfo

  # Depth cameras (Orbbec, Kinect, Azure Kinect, RealSense). Without submodules
  # on purpose: the repository vendors four camera SDKs that together take
  # longer to build than the rest of score, and none of them belong in this
  # build. The plug-in reaches cameras through a pure-C ABI and the SDKs ship as
  # a separate downloadable package, built from the addon's own repository.
  NO_SUBMODULES=1 clone_addon https://github.com/ossia/score-addon-orbbec
fi

if [[ "$CI_PLATFORM" == "LINUX" || "$CI_PLATFORM" == "WIN32" ]]; then
  clone_addon https://github.com/ossia/score-addon-librediffusion
fi
)



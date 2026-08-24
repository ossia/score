#!/usr/bin/env bash
# Sourced by ci/*.deps.sh and run directly by .github/workflows/bsd.yml, so it
# cannot rely on the caller being strict: src/addons/CMakeLists.txt globs
# whatever is present, so a failed clone or checkout yields a green build of the
# wrong tree.

(
set -euo pipefail

CI_PLATFORM="${1:-}"
case "${CI_PLATFORM}" in
  LINUX | MACOS | WIN32 | WASM | FREEBSD) ;;
  *)
    echo "common.deps.sh: missing or unknown platform '${CI_PLATFORM}'" >&2
    echo "  usage: source ci/common.deps.sh <LINUX|MACOS|WIN32|WASM|FREEBSD>" >&2
    exit 1
    ;;
esac

cd src/addons

# Containers and the FreeBSD NFS share run git as another uid than the checkout owner.
if [[ -n "${GITHUB_ACTIONS:-}${CI:-}" ]]; then
  git config --global --add safe.directory '*'
fi

REQUESTED=()

checkout_ref() {
  local url=${1} ref=${2}

  if ! git fetch --force origin "${ref}"; then
    echo "error: ${url}: no such ref '${ref}' on the remote" >&2
    return 1
  fi
  git checkout --force --detach FETCH_HEAD
  # The ref may add or move submodules relative to the default branch that was
  # cloned recursively above: sync them or the addon builds with the wrong trees.
  git submodule sync --recursive
  git submodule update --init --recursive --jobs 16
}

clone_addon() {
  local url=${1}
  local ref=${2:-}
  local folder
  folder=$(echo "${url}" | awk -F'/' '{print $NF}')

  REQUESTED+=("${folder}|${ref}|${url}")

  # Shallow in CI (throwaway checkouts); full history locally so git blame works.
  local shallow=()
  if [[ -n "${GITHUB_ACTIONS:-}${CI:-}" && -z "${ref}" ]]; then
    shallow=(--depth 1 --shallow-submodules)
  fi
  # ${arr[@]+"${arr[@]}"}: bash 3.2 (/bin/bash on macOS) sees "${empty[@]}" as unbound under -u.

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
      local sdepth=()
      if [[ ${#shallow[@]} -gt 0 ]]; then
        sdepth=(--depth 1)
      fi
      git clone ${sdepth[@]+"${sdepth[@]}"} "$url" "$folder"
      (
        cd "$folder"
        git submodule update --init ${sdepth[@]+"${sdepth[@]}"} "${SKIP_SUBMODULE%% *}"
        git -C "${SKIP_SUBMODULE%% *}" config "submodule.${SKIP_SUBMODULE#* }.update" none
        git submodule update --init --recursive ${sdepth[@]+"${sdepth[@]}"}
      )
    else
      git clone --recursive -j16 ${shallow[@]+"${shallow[@]}"} "$url" "$folder"
    fi

    if [[ -n "${ref}" ]]; then
      cd "$folder"
      checkout_ref "$url" "$ref"
    fi
  else
    # Try to update the addon if it's really super clean
    cd "$folder"
    git update-index --really-refresh || true
    if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
      if [[ -n "${ref}" ]]; then
        echo "error: ${folder} has local modifications, refusing to check out '${ref}'" >&2
        exit 1
      fi
      echo "note: ${folder} has local modifications, leaving it as-is" >&2
    elif [[ -n "${ref}" ]]; then
      checkout_ref "$url" "$ref"
    else
      git pull --ff-only || echo "note: could not update ${folder}, using the local checkout" >&2
      git submodule sync --recursive
      git submodule update --init --recursive --jobs 16
    fi
  fi
  )
}

verify_addons() {
  local failed=()
  local entry folder ref url head want

  echo "score add-ons:"
  for entry in ${REQUESTED[@]+"${REQUESTED[@]}"}; do
    folder=${entry%%|*}
    ref=${entry#*|}; ref=${ref%%|*}
    url=${entry##*|}

    if [[ ! -d "${folder}" ]]; then
      failed+=("${folder}: not cloned")
      continue
    fi
    if [[ ! -f "${folder}/CMakeLists.txt" ]]; then
      failed+=("${folder}: incomplete clone, no CMakeLists.txt")
      continue
    fi

    head=$(git -C "${folder}" rev-parse HEAD)
    if [[ -n "${ref}" ]]; then
      want=$(git ls-remote "${url}" "${ref}" | awk 'NR == 1 { print $1 }')
      want=${want:-${ref}}
      if [[ "${head}" != "${want}"* ]]; then
        failed+=("${folder}: on ${head}, expected ${want} (${ref})")
        continue
      fi
    fi
    printf '  %-34s %s %s\n' "${folder}" "${head:0:12}" "${ref}"
  done

  if [[ ${#failed[@]} -gt 0 ]]; then
    echo "error: the following add-ons were not set up correctly:" >&2
    printf '  %s\n' "${failed[@]}" >&2
    return 1
  fi
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

if [[ "$CI_PLATFORM" != "WASM" ]];
then
  clone_addon          https://github.com/ossia/score-addon-ble
  clone_addon  https://github.com/ossia/score-addon-contextfree
  clone_addon    https://github.com/ossia/score-addon-deuterium
  clone_addon         https://github.com/ossia/score-addon-hdf5
  clone_addon          https://github.com/ossia/score-addon-led
  clone_addon          https://github.com/ossia/score-addon-lsl
  clone_addon          https://github.com/ossia/score-addon-ndi
  clone_addon      https://github.com/ossia/score-addon-openzen
  clone_addon     https://github.com/ossia/score-addon-spatgris
  clone_addon    https://github.com/ossia/score-addon-ultraleap
  clone_addon      https://github.com/ossia/score-addon-sysinfo
  clone_addon https://github.com/sat-mtl/carto-tcp-avendish.git update-avendish-packaging
  NO_SUBMODULES=1 clone_addon https://github.com/ossia/score-addon-orbbec
fi

if [[ "$CI_PLATFORM" == "LINUX" || "$CI_PLATFORM" == "WIN32" ]]; then
  clone_addon https://github.com/ossia/score-addon-librediffusion
fi

verify_addons
)

score_addons_status=$?
if [[ ${score_addons_status} -ne 0 ]]; then
  echo "error: failed to set up the score add-ons, see above" >&2
  exit ${score_addons_status}
fi

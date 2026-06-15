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

CI_PLATFORM="${1:-DEFAULT}"

if [[ "$CI_PLATFORM" != "WASM" ]];
then

  clone_addon https://github.com/ossia/score-addon-onnx
  ( cd score-addon-onnx ; git checkout feature/image-processor )
fi

)



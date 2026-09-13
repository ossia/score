#!/bin/bash
### Here and not on the shebang, which is ignored when CI calls this as
### `bash ci/sdk-smoke.sh` -- and a smoke test that cannot fail is worse than none.
set -euo pipefail

### Does the SDK this build just produced actually build an add-on?
###
### Usage: ci/sdk-smoke.sh <sdk-zip-or-dir> [workdir]
### Requires OSSIA_SDK; honours SCORE_SMOKE_ADDON_URL / SCORE_SMOKE_ADDON_DIR.

SDK_INPUT="${1:?usage: ci/sdk-smoke.sh <sdk-zip-or-dir> [workdir]}"
WORKDIR="${2:-${PWD}/sdk-smoke}"
: "${OSSIA_SDK:?OSSIA_SDK must point at the ossia SDK}"
ADDON_URL="${SCORE_SMOKE_ADDON_URL:-https://github.com/ossia-templates/score-avnd-simple-template}"

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"

# Accept either the published zip or an already-unpacked tree, so this can run
# against the deploy artifact in CI and against a build directory locally.
### Resolve before any cd. unzip is not a given -- the build container has only
### python3, and the Windows scripts all use 7z.
extract_sdk_zip() {
  local zip=$1 dest=$2
  if command -v unzip >/dev/null 2>&1; then
    (cd "$dest" && unzip -qq "$zip")
  elif command -v 7z >/dev/null 2>&1; then
    7z x -bso0 -bsp0 -o"$dest" "$zip"
  elif command -v bsdtar >/dev/null 2>&1; then
    bsdtar -xf "$zip" -C "$dest"
  elif command -v python3 >/dev/null 2>&1; then
    python3 -m zipfile -e "$zip" "$dest"
  else
    echo "error: no way to unpack '$zip' -- need unzip, 7z, bsdtar or python3" >&2
    return 1
  fi
}

if [[ -d "$SDK_INPUT" ]]; then
  SDK_ROOT="$SDK_INPUT"
else
  SDK_ZIP="$(cd "$(dirname "$SDK_INPUT")" && pwd)/$(basename "$SDK_INPUT")"
  extract_sdk_zip "$SDK_ZIP" "$WORKDIR"
  SDK_ROOT="$WORKDIR/usr"
fi

if [[ ! -d "$SDK_ROOT/lib/cmake/score" ]]; then
  echo "error: '$SDK_ROOT' does not look like a score SDK (no lib/cmake/score)" >&2
  exit 1
fi

### The add-on compiler runs with -nostdinc against the SDK's own headers, so a
### missing toolchain tree is fatal in a way no CMake error explains well.
case "$(uname -s)" in
  Darwin) EXPECTED_ARCH_OS=darwin ;;
  MINGW*|MSYS*|CYGWIN*) EXPECTED_ARCH_OS=windows ;;
  FreeBSD) EXPECTED_ARCH_OS=freebsd ;;
  *) EXPECTED_ARCH_OS=linux ;;
esac
case "$(uname -m)" in
  x86_64|amd64) EXPECTED_ARCH_CPU=x86_64 ;;
  arm64|aarch64) EXPECTED_ARCH_CPU=aarch64 ;;
  *) EXPECTED_ARCH_CPU="$(uname -m)" ;;
esac
EXPECTED_ARCH="${EXPECTED_ARCH_OS}-${EXPECTED_ARCH_CPU}"

if [[ -n "${SCORE_SMOKE_ADDON_DIR:-}" ]]; then
  ADDON_SRC="$SCORE_SMOKE_ADDON_DIR"
else
  ADDON_SRC="$WORKDIR/addon"
  git clone --quiet --depth 1 --recursive "$ADDON_URL" "$ADDON_SRC"

  # The template ships the nil uuid for init.sh to replace, and the assertion below
  # rejects it. Substitute it here rather than running init.sh, which needs
  # perl-rename and friends.
  find "$ADDON_SRC" -type f \( -name '*.hpp' -o -name '*.cpp' -o -name '*.json' -o -name '*.txt' \) \
    -exec sed -i.bak "s/00000000-0000-0000-0000-000000000000/5c0de5dc-5dc0-4de5-9c0d-e55dc0de5dc0/g" {} +
  find "$ADDON_SRC" -name '*.bak' -delete
fi

CMAKE_COMPILERS=()
if [[ -x "$OSSIA_SDK/llvm/bin/clang++" ]]; then
  CMAKE_COMPILERS=(
    "-DCMAKE_C_COMPILER=$OSSIA_SDK/llvm/bin/clang"
    "-DCMAKE_CXX_COMPILER=$OSSIA_SDK/llvm/bin/clang++")
  export PATH="$OSSIA_SDK/llvm/bin:$PATH"
fi

cmake -S "$ADDON_SRC" -B "$WORKDIR/build" -GNinja \
  -DCMAKE_MODULE_PATH="$SDK_ROOT/lib/cmake/score" \
  -DSCORE_SDK="$SDK_ROOT" \
  -DOSSIA_SDK="$OSSIA_SDK" \
  -DCMAKE_BUILD_TYPE=Release \
  "${CMAKE_COMPILERS[@]}"

cmake --build "$WORKDIR/build"

MANIFEST=$(find "$WORKDIR/build" -name localaddon.json | head -1)
if [[ -z "$MANIFEST" ]]; then
  echo "error: the add-on built but produced no localaddon.json, so it could never be installed" >&2
  exit 1
fi

### makeAddon() discards an add-on whose key score::addonArchitecture() does not
### compute, with no diagnostic, so check the string and not just the file.
### And just as silently when the key is not a 36-character uuid. The nil one is
### what an uninitialised template ships.
MANIFEST_UUID=$(sed -n 's/.*"key"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$MANIFEST" | head -1)
if [[ ${#MANIFEST_UUID} -ne 36 || "$MANIFEST_UUID" == "00000000-0000-0000-0000-000000000000" ]]; then
  echo "error: localaddon.json key '${MANIFEST_UUID}' is not a usable uuid -- score would ignore this add-on" >&2
  cat "$MANIFEST" >&2
  exit 1
fi

if ! grep -q "\"${EXPECTED_ARCH}\"" "$MANIFEST"; then
  echo "error: localaddon.json has no '${EXPECTED_ARCH}' key -- score would silently ignore this add-on:" >&2
  cat "$MANIFEST" >&2
  exit 1
fi

echo "sdk-smoke: OK -- built against $SDK_ROOT, manifest advertises ${EXPECTED_ARCH}"
cat "$MANIFEST"

#!/bin/bash -e
# Build an ossia score Yocto image locally.
#
#   ./build.sh                              # qemux86-64, ossia-score-image
#   ./build.sh whinlatter-raspberrypi4-64   # a different kas config
#   ./build.sh whinlatter-qemux86-64 shell  # drop into a bitbake shell
#
# Requires kas (pip install kas) and the host packages listed at
# https://docs.yoctoproject.org/ref-manual/system-requirements.html
#
# Expect the first build to take several hours and 90-120 GB of disk. Set
# OSSIA_YOCTO_BUILD_DIR, DL_DIR and SSTATE_DIR to persistent locations if you
# intend to iterate -- see the README.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCORE_DIR="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

CONFIG="${1:-whinlatter-qemux86-64}"
COMMAND="${2:-build}"

KAS_FILE="$SCRIPT_DIR/kas/$CONFIG.yml"
if [[ ! -f "$KAS_FILE" ]]; then
  echo "No such kas config: $KAS_FILE" >&2
  echo "Available:" >&2
  find "$SCRIPT_DIR/kas" -maxdepth 1 -name '*.yml' -exec basename {} .yml \; | sed 's/^/  /' >&2
  exit 1
fi

export KAS_WORK_DIR="${OSSIA_YOCTO_WORK_DIR:-$SCORE_DIR/../score-yocto}"
export KAS_BUILD_DIR="${OSSIA_YOCTO_BUILD_DIR:-$KAS_WORK_DIR/build}"
mkdir -p "$KAS_WORK_DIR" "$KAS_BUILD_DIR"

# kas resolves the meta-ossia repo entry relative to the checkout it is run
# from, so point it at the score tree rather than the Yocto directory.
cd "$SCORE_DIR"

echo "score:  $SCORE_DIR"
echo "config: $CONFIG"
echo "work:   $KAS_WORK_DIR"
echo "build:  $KAS_BUILD_DIR"

# --target so ossia-score-image-appliance is reachable without editing YAML.
exec kas "$COMMAND" ${OSSIA_YOCTO_TARGET:+--target "$OSSIA_YOCTO_TARGET"} "$KAS_FILE"

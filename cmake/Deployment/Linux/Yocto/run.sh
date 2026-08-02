#!/bin/bash -e
# Boot a built image in qemu and open a viewer on it.
#
#   ./run.sh                              # qemux86-64, score starts on boot
#   OSSIA_QEMU_MEM=4096 ./run.sh          # more RAM
#   OSSIA_QEMU_AUTOSTART=0 ./run.sh       # boot to a shell instead
#   OSSIA_QEMU_IMAGE=ossia-score-image-appliance ./run.sh
#
# VNC, not -display sdl,gl=on: where the host cannot import the guest scanout
# dmabuf (proprietary NVIDIA) the window silently freezes on a stale frame.
#
# Ctrl-C, or closing the viewer, shuts the VM down.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCORE_DIR="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

CONFIG="${1:-whinlatter-qemux86-64}"
KAS_FILE="$SCRIPT_DIR/kas/$CONFIG.yml"
if [[ ! -f "$KAS_FILE" ]]; then
  echo "No such kas config: $KAS_FILE" >&2
  find "$SCRIPT_DIR/kas" -maxdepth 1 -name '*.yml' -exec basename {} .yml \; | sed 's/^/  /' >&2
  exit 1
fi

MEM="${OSSIA_QEMU_MEM:-2048}"
# runqemu defaults to 4; score runs 8 threads at startup and benefits from more.
SMP="${OSSIA_QEMU_SMP:-$(nproc 2>/dev/null || echo 4)}"
IMAGE="${OSSIA_QEMU_IMAGE:-ossia-score-image}"
export KAS_WORK_DIR="${OSSIA_YOCTO_WORK_DIR:-$SCORE_DIR/../score-yocto}"
export KAS_BUILD_DIR="${OSSIA_YOCTO_BUILD_DIR:-$KAS_WORK_DIR/build-$CONFIG}"

VIEWER=""
for v in gvncviewer vncviewer xtightvncviewer; do
  if command -v "$v" >/dev/null 2>&1; then VIEWER="$v"; break; fi
done

# 5900 + N. Pick one nothing is sitting on.
VNC_N=0
for n in $(seq 1 32); do
  if ! (exec 3<>"/dev/tcp/127.0.0.1/$((5900 + n))") 2>/dev/null; then VNC_N=$n; break; fi
done
if [[ "$VNC_N" == 0 ]]; then
  echo "No free VNC display between :1 and :32" >&2
  exit 1
fi
VNC_PORT=$((5900 + VNC_N))

# runqemu builds its own kernel command line and ignores the image's APPEND, so
# the distro's quiet settings have to be repeated here to have any effect under
# qemu. Set OSSIA_VERBOSE_BOOT=1 to see the boot log instead.
BP=""
if [[ "${OSSIA_VERBOSE_BOOT:-0}" != "1" ]]; then
  BP="quiet loglevel=3 systemd.show_status=0"
fi
if [[ "$IMAGE" == *appliance* ]]; then
  BP="$BP init=/usr/bin/ossia-score-init"
elif [[ "${OSSIA_QEMU_AUTOSTART:-1}" == "1" ]]; then
  BP="$BP systemd.wants=ossia-score.service"
fi
BOOTPARAMS=""
if [[ -n "${BP// /}" ]]; then
  BOOTPARAMS="bootparams='$BP'"
fi

echo "image  : $IMAGE ($CONFIG)"
echo "memory : ${MEM}M, ${SMP} vCPU"
echo "vnc    : localhost:$VNC_N (port $VNC_PORT)"
echo "ssh    : ssh -p 2222 root@127.0.0.1"

cd "$SCORE_DIR"

# ext4.zst explicitly: the image also produces tar.zst and wic, and runqemu
# picks between them unpredictably when the format is left off.
kas shell "$KAS_FILE" -c \
  "runqemu kvm slirp egl-headless snapshot $IMAGE ext4.zst \
     qemuparams='-m $MEM -smp $SMP -vnc :$VNC_N' $BOOTPARAMS" &
QEMU_KAS_PID=$!

cleanup() {
  # Only our own qemu: pkill -x would take out unrelated VMs on the host.
  local kids
  kids=$(pgrep -P "$QEMU_KAS_PID" 2>/dev/null)
  kill "$QEMU_KAS_PID" 2>/dev/null || true
  for k in $kids; do
    pkill -P "$k" -x qemu-system-x86 2>/dev/null || true
  done
}
trap cleanup EXIT INT TERM

for _ in $(seq 1 60); do
  if (exec 3<>"/dev/tcp/127.0.0.1/$VNC_PORT") 2>/dev/null; then break; fi
  if ! kill -0 "$QEMU_KAS_PID" 2>/dev/null; then
    echo "qemu exited before the VNC port opened" >&2
    exit 1
  fi
  sleep 1
done

if [[ -z "$VIEWER" ]]; then
  echo
  echo "No VNC viewer found. Install one (e.g. gvncviewer) or connect to"
  echo "  localhost:$VNC_N"
  echo "Ctrl-C to shut the VM down."
  wait "$QEMU_KAS_PID"
else
  echo "opening $VIEWER..."
  "$VIEWER" "localhost:$VNC_N" || true
fi

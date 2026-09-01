#!/usr/bin/env bash
# Mirror the FFmpeg FATE sample suite — the reference corpus of files in every
# codec/container ffmpeg supports, including deliberately damaged ones.
# ~1.3 GB. Re-running only transfers what changed.
set -euo pipefail
DEST="${1:-$HOME/ossia/video-corpus/fate-suite}"
mkdir -p "$DEST"
exec rsync -a --info=progress2 rsync://fate-suite.ffmpeg.org/fate-suite/ "$DEST/"

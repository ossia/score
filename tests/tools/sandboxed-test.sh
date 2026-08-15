#!/bin/sh
# Run a test binary with the whole filesystem read-only except for scratch space.
#
# Used for the tests that exercise code which deletes or overwrites media
# files. Those tests only ever work inside a QTemporaryDir, but "only ever" is
# a claim about code that is being changed; this makes it a property of the
# environment instead. A bug that tries to remove one of the developer's own
# files gets EROFS rather than removing it.
#
# Writable inside the sandbox, and nowhere else:
#   /tmp                 -- fresh tmpfs, where QTemporaryDir puts everything
#   ~/.config ~/.cache ~/.local/share  -- fresh tmpfs, so Qt settings and
#                           caches never touch the real ones
#   /run/user            -- fresh tmpfs
#
# Usage: sandboxed-test.sh <binary> [args...]
#
# Falls through to running the binary directly when bubblewrap is missing, so
# a machine without it still runs the suite -- just without the net.

set -eu

if [ $# -lt 1 ]; then
  echo "usage: $0 <binary> [args...]" >&2
  exit 2
fi

if ! command -v bwrap >/dev/null 2>&1; then
  echo "note: bubblewrap not found, running $1 unsandboxed" >&2
  exec "$@"
fi

exec bwrap \
  --ro-bind / / \
  --dev /dev \
  --proc /proc \
  --tmpfs /tmp \
  --tmpfs "${HOME}/.config" \
  --tmpfs "${HOME}/.cache" \
  --tmpfs "${HOME}/.local/share" \
  --tmpfs /run/user \
  --setenv TMPDIR /tmp \
  --setenv XDG_RUNTIME_DIR /run/user \
  --setenv SCORE_TEST_SANDBOXED 1 \
  --die-with-parent \
  -- "$@"

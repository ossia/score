#!/bin/sh
# Hardware probe: is a Blackmagic DeckLink usable?
# The harness talks to the card through the DesktopVideo driver stack, whose
# kernel module exposes /dev/blackmagic/. Requiring the device nodes (not just
# a PCI match) means "card present AND driver installed", which is what the
# DeckLink API needs to enumerate anything.
# Exit 0 = present, non-zero = absent.
set -u

for d in /dev/blackmagic/*; do
  [ -e "$d" ] && exit 0
done

# Older DesktopVideo versions used flat /dev/blackmagic! nodes.
[ -d /sys/module/blackmagic ] && exit 0

exit 1

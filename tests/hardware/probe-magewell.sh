#!/bin/sh
# Hardware probe: is a Magewell capture card present?
# PCI vendor id 0x1cd7 (Magewell). sysfs check first (no lspci dependency),
# lspci as fallback for unusual sysfs layouts.
# Exit 0 = present, non-zero = absent.
set -u

for v in /sys/bus/pci/devices/*/vendor; do
  [ "$(cat "$v" 2>/dev/null)" = "0x1cd7" ] && exit 0
done

if command -v lspci >/dev/null 2>&1; then
  [ -n "$(lspci -d 1cd7: 2>/dev/null)" ] && exit 0
fi

exit 1

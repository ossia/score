SUMMARY = "Bare-bones image running ossia score"
DESCRIPTION = "A minimal bootable image whose only purpose is to run ossia \
score: kernel, init, audio and GPU stacks, networking, and score itself with \
every plugin and addon that cross-compiles. No desktop environment."
LICENSE = "MIT"

inherit core-image

# ssh so the box can be driven headlessly; no dev/debug packages, this is meant
# to be flashed rather than developed on. Add "debug-tweaks" locally if you want
# a passwordless root console while bringing a board up.
IMAGE_FEATURES += "ssh-server-openssh"

IMAGE_INSTALL += " \
    ossia-score \
    alsa-utils \
    alsa-plugins \
    pipewire \
    pipewire-alsa \
    wireplumber \
    kernel-modules \
    e2fsprogs-mke2fs \
    tzdata \
"

# Not stripped down: the point of this image is the full score feature set. It
# lands somewhere north of 500 MB, dominated by Qt and the score binary itself.
IMAGE_OVERHEAD_FACTOR = "1.3"
IMAGE_ROOTFS_EXTRA_SPACE = "524288"

# Flashable artefacts, in addition to whatever the machine already produces
# (qemux86-64 gives ext4.zst + tar.zst, which is what runqemu wants). bmap lets
# bmaptool skip the empty blocks when writing to a card or SSD; the machine's
# WKS_FILE decides the partition layout. Appended rather than assigned so this
# does not fight a machine that has its own opinion -- meta-raspberrypi, for
# one; the Pi kas config overrides the whole variable with :forcevariable.
IMAGE_FSTYPES:append = " wic.xz wic.bmap"

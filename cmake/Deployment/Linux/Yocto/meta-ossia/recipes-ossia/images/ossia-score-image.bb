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

# No PipeWire: score talks to ALSA directly and nothing here needs a sound
# server. Re-add with IMAGE_INSTALL:append and PACKAGECONFIG:append:pn-ossia-score.
IMAGE_INSTALL += " \
    ossia-score \
    alsa-utils \
    alsa-plugins \
    kernel-modules \
    e2fsprogs-mke2fs \
    tzdata \
    ca-certificates \
    systemd-ossia-ordering \
"

# systemd-resolved and systemd-timesyncd use PrivateTmp= but order themselves
# after nothing that guarantees /var/volatile/tmp exists; on this image
# systemd-tmpfiles-setup finishes at ~2.4s while they start at ~1.4s, so on a
# read-only rootfs they fail 226/NAMESPACE and the system comes up degraded.
# systemd-ossia-ordering ships the drop-ins that close that race.

# Note on sizing: score's measured RSS is ~312 MB once the UI is up, before any
# document is loaded. Below roughly 512 MB it does not merely run slowly, it
# dies -- at runqemu's 256 MB default it segfaults during startup and the
# restarted instance is OOM-killed. Budget 1 GB or more on real hardware.

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

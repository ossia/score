# linux-yocto only: the Pi machines use linux-raspberrypi and are unaffected.
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://ossia-trim.cfg"

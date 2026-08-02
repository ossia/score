SUMMARY = "systemd drop-ins for the ossia score appliance"
DESCRIPTION = "Drop-ins ordering /var-dependent systemd units after \
tmpfiles-setup, and disabling autovt."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://private-tmp-after-volatile.conf file://no-autovt.conf"
S = "${UNPACKDIR}"

inherit features_check allarch
REQUIRED_DISTRO_FEATURES = "systemd"

# Units that need /var/lib or /var/volatile before they can start.
OSSIA_VOLATILE_ORDERED_UNITS ?= "systemd-resolved.service systemd-timesyncd.service systemd-networkd-persistent-storage.service"

do_install() {
    for unit in ${OSSIA_VOLATILE_ORDERED_UNITS}; do
        install -d ${D}${systemd_system_unitdir}/${unit}.d
        install -m 0644 ${UNPACKDIR}/private-tmp-after-volatile.conf \
            ${D}${systemd_system_unitdir}/${unit}.d/10-ossia-volatile.conf
    done

    install -d ${D}${sysconfdir}/systemd/logind.conf.d
    install -m 0644 ${UNPACKDIR}/no-autovt.conf \
        ${D}${sysconfdir}/systemd/logind.conf.d/10-ossia-no-autovt.conf

    install -d ${D}${sysconfdir}/systemd/system
    ln -sf /dev/null ${D}${sysconfdir}/systemd/system/getty@tty1.service
}

FILES:${PN} = "${systemd_system_unitdir} ${sysconfdir}/systemd"

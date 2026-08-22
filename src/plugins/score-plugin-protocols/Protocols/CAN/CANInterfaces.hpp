#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include <QString>
#include <QStringList>

#include <vector>

namespace Protocols::CAN
{

/**
 * One SocketCAN network interface present on the machine.
 */
struct InterfaceInfo
{
  //! netdev name: "can0", "vcan0", "slcan0", or whatever a udev rule renamed it to.
  QString name;

  //! The interface accepts CAN FD frames (its MTU is at least CANFD_MTU).
  bool fdCapable{false};
};

/**
 * List the CAN interfaces of the machine, sorted by name.
 *
 * Matched on the link type (ARPHRD_CAN in /sys/class/net/<if>/type), not the
 * name: a udev-renamed USB adapter is still a CAN interface. The name prefixes
 * are only a fallback for when sysfs cannot be read.
 *
 * Empty on a machine with no CAN hardware and no vcan - the common case, and
 * not an error.
 */
std::vector<InterfaceInfo> availableInterfaces();

//! Just the names, in the same order.
QStringList availableInterfaceNames();

/**
 * The interface a new CAN device should default to, or an empty string.
 *
 * Empty is a deliberate outcome, not a failure: a default of "can0" on a
 * machine that has no can0 is wrong on every machine without a physical CAN
 * port, and produces "no such CAN interface: can0" at connection time for a
 * setting the user never chose.
 */
QString defaultInterface();

}
#endif

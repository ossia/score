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
 * Matched on the link type (ARPHRD_CAN, as sysfs reports it in
 * /sys/class/net/<if>/type) rather than on the name: an interface does not have
 * to be called "canN" -- a USB adapter renamed by a udev rule is still a CAN
 * interface -- and conversely a name is not proof of anything. The name
 * prefixes are kept only as a fallback for the case where sysfs cannot be read.
 *
 * Returns an empty list on a machine with no CAN hardware and no vcan module,
 * which is the common case: that is not an error and callers must handle it.
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

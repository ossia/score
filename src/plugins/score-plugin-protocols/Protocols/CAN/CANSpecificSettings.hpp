#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include <score/tools/std/StringHash.hpp>

#include <QString>

#include <verdigris>

namespace Protocols
{

struct CANSpecificSettings
{
  //! The netdev name of the CAN interface: "can0", "vcan0", "slcan0"...
  //!
  //! Deliberately not spelled `interface`: that is a macro on Windows, pulled
  //! in by <objbase.h> through most of the Win32 headers. The field is
  //! unreachable there anyway, but the name would still break the build of any
  //! translation unit that happens to include both.
  QString interfaceName;

  //! Absolute path to the .dbc database describing the frames on the bus.
  QString dbcPath;

  /**
   * Added to every message identifier read from the database.
   *
   * A DBC usually hardcodes the identifiers of one physical device. The LPMS
   * files, for instance, are written for CANopen node 1: their frames are
   * 0x181/0x281/0x381/0x481 (the CANopen "start id + node id" convention) plus
   * the 0x701 heartbeat. The very same sensor configured as node 2 puts the
   * same signals on 0x182/0x282/... -- the layout is identical, only the base
   * changes.
   *
   * Rather than requiring one edited copy of the vendor file per sensor, the
   * offset shifts the whole database at load time, so a chain of N devices on
   * one bus is N score devices pointing at one file with offsets 0, 1, 2...
   *
   * Signed: a database written for node 3 can be pulled back to node 1 with -2.
   */
  int nodeIdOffset{0};

  /**
   * Decode every 32-bit integer signal as an IEEE 754 single instead.
   *
   * Off by default, and it must stay that way: it contradicts what the file
   * says, and applying it to a database that means what it says turns good
   * data into noise.
   *
   * It exists because some vendor files are simply wrong. LPMS3_32bit.dbc
   * declares its signals `32@1-` with a `(1,0)` scaling and carries no
   * SIG_VALTYPE_ record, yet the sensor's own command set
   * (SET_CAN_DATA_PRECISION, 0x72) documents mode 1 as "32bit floating point".
   * Decoded as written, a quaternion component of 1.0 reads as 1065353216.
   *
   * Signals that carry an explicit SIG_VALTYPE_ float or double type are never
   * touched by this: an explicit statement in the file outranks the override.
   */
  bool float32Override{false};

  /**
   * Enable CAN FD (payloads above 8 bytes).
   *
   * Not a mode switch: classic frames keep arriving on the same socket.
   */
  bool fd{false};

  /**
   * Ask the kernel to drop every frame whose identifier is not in the database.
   *
   * SocketCAN applies filters per socket, so each device of a chain sees only
   * its own node's frames and the others cost it nothing. Worth turning off
   * only when debugging a bus whose traffic does not match the file.
   */
  bool filterToDatabase{true};
};
}

Q_DECLARE_METATYPE(Protocols::CANSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::CANSpecificSettings)
#endif

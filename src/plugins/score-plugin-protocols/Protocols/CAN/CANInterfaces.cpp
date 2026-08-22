#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include "CANInterfaces.hpp"

#include <QDir>
#include <QFile>

namespace Protocols::CAN
{
namespace
{
//! ARPHRD_CAN, from <linux/if_arp.h>. Read out of sysfs rather than included,
//! so that this does not pull a kernel header in for one constant.
constexpr int arphrdCan = 280;

//! CANFD_MTU, from <linux/can.h>: 72 bytes (the 64-byte payload plus the
//! canfd_frame header). A classic-only controller stays at CAN_MTU == 16.
constexpr int canfdMtu = 72;

int readIntFile(const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return -1;

  bool ok = false;
  const int v = f.readAll().trimmed().toInt(&ok);
  return ok ? v : -1;
}
}

std::vector<InterfaceInfo> availableInterfaces()
{
  std::vector<InterfaceInfo> out;

  QDir sys{"/sys/class/net"};
  auto entries = sys.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
  entries.sort();

  for(const auto& name : entries)
  {
    const int type = readIntFile("/sys/class/net/" + name + "/type");

    bool isCan{};
    if(type >= 0)
      isCan = (type == arphrdCan);
    else
      isCan = name.startsWith("can") || name.startsWith("vcan")
              || name.startsWith("slcan");

    if(!isCan)
      continue;

    out.push_back(
        InterfaceInfo{
            .name = name,
            .fdCapable = readIntFile("/sys/class/net/" + name + "/mtu") >= canfdMtu});
  }

  return out;
}

QStringList availableInterfaceNames()
{
  QStringList out;
  for(const auto& itf : availableInterfaces())
    out.push_back(itf.name);
  return out;
}

QString defaultInterface()
{
  // Real interfaces before virtual ones: on a machine that has both a physical
  // adapter and a vcan left over from a test, the adapter is what the user
  // means. Within each group the sysfs order (alphabetical) decides.
  QString virtualCandidate;
  for(const auto& itf : availableInterfaces())
  {
    if(itf.name.startsWith("vcan"))
    {
      if(virtualCandidate.isEmpty())
        virtualCandidate = itf.name;
    }
    else
    {
      return itf.name;
    }
  }
  return virtualCandidate;
}

}
#endif

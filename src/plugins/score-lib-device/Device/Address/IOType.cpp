// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IOType.hpp"

#include <ossia/detail/enum_map.hpp>

#include <QObject>
#include <QString>

namespace Device
{
const ossia::enum_map<ossia::access_mode, QString, 3>& AccessModeText()
{
  static const ossia::enum_map<ossia::access_mode, QString, 3> iotypemap{
      {ossia::access_mode::GET, QObject::tr("<-")},
      {ossia::access_mode::SET, QObject::tr("->")},
      {ossia::access_mode::BI, QObject::tr("<->")}};
  return iotypemap;
}
const ossia::enum_map<ossia::access_mode, QString, 3>& AccessModePrettyText()
{
  static const ossia::enum_map<ossia::access_mode, QString, 3> iotypemap{
      {ossia::access_mode::GET, QObject::tr("<- (get)")},
      {ossia::access_mode::SET, QObject::tr("-> (set)")},
      {ossia::access_mode::BI, QObject::tr("<-> (bi)")}};
  return iotypemap;
}
}

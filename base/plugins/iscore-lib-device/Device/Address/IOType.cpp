// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QMap>
#include <QObject>

#include <QString>

#include "IOType.hpp"

namespace Device
{
const QMap<ossia::access_mode, QString>& AccessModeText()
{
  static const QMap<ossia::access_mode, QString> iotypemap{
    {
      {ossia::access_mode::GET, QObject::tr("<-")},
      {ossia::access_mode::SET, QObject::tr("->")},
      {ossia::access_mode::BI, QObject::tr("<->")}
    }};
  return iotypemap;
}
}

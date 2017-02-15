#include <QMap>
#include <QObject>

#include <QString>

#include "IOType.hpp"

namespace Device
{

static const QMap<ossia::access_mode, QString> iotypemap{
  {
    {ossia::access_mode::GET, QObject::tr("<-")},
    {ossia::access_mode::SET, QObject::tr("->")},
    {ossia::access_mode::BI, QObject::tr("<->")}
  }};

const QMap<ossia::access_mode, QString>& AccessModeText()
{
  return iotypemap;
}
}

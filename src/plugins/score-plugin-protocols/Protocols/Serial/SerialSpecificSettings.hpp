#pragma once

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)
#include <QSerialPortInfo>
#include <QString>

#include <verdigris>

namespace Protocols
{
struct SerialSpecificSettings
{
  QSerialPortInfo port;
  QString text;
  int32_t rate{0};
};
}
Q_DECLARE_METATYPE(Protocols::SerialSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::SerialSpecificSettings)

#endif

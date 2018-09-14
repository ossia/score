#pragma once

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SERIAL)
#include <QMetaType>
#include <QSerialPortInfo>
#include <QString>

#include <wobjectdefs.h>

namespace Engine
{
namespace Network
{
struct SerialSpecificSettings
{
  QSerialPortInfo port;
  QString text;
};
}
}
Q_DECLARE_METATYPE(Engine::Network::SerialSpecificSettings)
W_REGISTER_ARGTYPE(Engine::Network::SerialSpecificSettings)

#endif

#pragma once
#include <QMetaType>
#include <QString>
#include <QSerialPortInfo>

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

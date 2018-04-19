#pragma once
#include <QMetaType>
#include <QSerialPortInfo>
#include <QString>

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

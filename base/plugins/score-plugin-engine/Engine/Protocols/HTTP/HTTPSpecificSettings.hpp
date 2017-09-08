#pragma once
#include <QMetaType>
#include <QString>

namespace Engine
{
namespace Network
{
struct HTTPSpecificSettings
{
  QString text;
};
}
}
Q_DECLARE_METATYPE(Engine::Network::HTTPSpecificSettings)

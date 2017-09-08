#pragma once
#include <QMetaType>
#include <QString>

namespace Engine
{
namespace Network
{
struct WSSpecificSettings
{
  QString address;
  QString text;
};
}
}
Q_DECLARE_METATYPE(Engine::Network::WSSpecificSettings)

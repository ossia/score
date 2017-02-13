#pragma once
#include <QMetaType>
#include <QString>

namespace Engine
{
namespace Network
{
struct OSCQuerySpecificSettings
{
  QString host;
};
}
}
Q_DECLARE_METATYPE(Engine::Network::OSCQuerySpecificSettings)

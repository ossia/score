#pragma once
#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>

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
W_REGISTER_ARGTYPE(Engine::Network::OSCQuerySpecificSettings)

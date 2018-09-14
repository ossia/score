#pragma once
#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>

namespace Engine
{
namespace Network
{
struct OSCSpecificSettings
{
  int inputPort{};
  int outputPort{};
  QString host;
};
}
}
Q_DECLARE_METATYPE(Engine::Network::OSCSpecificSettings)
W_REGISTER_ARGTYPE(Engine::Network::OSCSpecificSettings)

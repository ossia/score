#pragma once
#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>

namespace Engine
{
namespace Network
{
struct LocalSpecificSettings
{
  int wsPort{};
  int oscPort{};
};
}
}
Q_DECLARE_METATYPE(Engine::Network::LocalSpecificSettings)
W_REGISTER_ARGTYPE(Engine::Network::LocalSpecificSettings)

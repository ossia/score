#pragma once
#include <QMetaType>
#include <QString>

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

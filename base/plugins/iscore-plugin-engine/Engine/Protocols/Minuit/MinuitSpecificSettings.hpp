#pragma once
#include <QMetaType>
#include <QString>

namespace Engine
{
namespace Network
{
struct MinuitSpecificSettings
{
  int inputPort{};
  int outputPort{};
  QString host;
};
}
}
Q_DECLARE_METATYPE(Engine::Network::MinuitSpecificSettings)

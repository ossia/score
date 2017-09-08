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
  QString localName;
};
}
}
Q_DECLARE_METATYPE(Engine::Network::MinuitSpecificSettings)

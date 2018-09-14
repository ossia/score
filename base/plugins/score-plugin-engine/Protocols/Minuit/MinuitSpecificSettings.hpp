#pragma once
#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>

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
W_REGISTER_ARGTYPE(Engine::Network::MinuitSpecificSettings)

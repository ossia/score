#pragma once
#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>

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
W_REGISTER_ARGTYPE(Engine::Network::HTTPSpecificSettings)

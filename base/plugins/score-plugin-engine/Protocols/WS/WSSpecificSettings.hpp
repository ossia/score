#pragma once
#include <QMetaType>
#include <QString>

#include <wobjectdefs.h>

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
W_REGISTER_ARGTYPE(Engine::Network::WSSpecificSettings)

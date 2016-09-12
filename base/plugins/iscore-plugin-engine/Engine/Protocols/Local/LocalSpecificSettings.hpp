#pragma once
#include <QString>
#include <QMetaType>

namespace Engine
{
namespace Network
{
struct LocalSpecificSettings
{
QString remoteName;
QString host;
int remotePort{};
int localPort{};
};
}
}
Q_DECLARE_METATYPE(Engine::Network::LocalSpecificSettings)

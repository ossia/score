#pragma once
#include <QString>
#include <QMetaType>

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

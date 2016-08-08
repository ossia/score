#pragma once
#include <QMetaType>
#include <QJsonObject>

namespace Engine
{
namespace Network
{
struct MIDISpecificSettings
{
        enum class IO { In, Out } io;
        QString endpoint;
        int port{};
};
}
}
Q_DECLARE_METATYPE(Engine::Network::MIDISpecificSettings)


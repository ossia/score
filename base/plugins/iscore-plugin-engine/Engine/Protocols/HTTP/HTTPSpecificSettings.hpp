#pragma once
#include <QString>
#include <QMetaType>

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

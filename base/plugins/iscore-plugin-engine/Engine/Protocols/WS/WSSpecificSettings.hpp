#pragma once
#include <QString>
#include <QMetaType>

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

#pragma once
#include <QMetaType>
#include <QJsonObject>

struct MIDISpecificSettings
{
        enum class IO { In, Out } io;
        QString endpoint;
};
Q_DECLARE_METATYPE(MIDISpecificSettings)


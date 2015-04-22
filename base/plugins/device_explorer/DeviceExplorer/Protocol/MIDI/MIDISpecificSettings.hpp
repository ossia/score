#pragma once
#include <QMetaType>
#include <QJsonObject>

struct MIDISpecificSettings
{
        enum class IO { In, Out } io;
};
Q_DECLARE_METATYPE(MIDISpecificSettings)

// TODO : pas de différents host possibles ?

#pragma once
#include <QMetaType>

struct MIDISpecificSettings
{
        enum class IO { In, Out } io;
};
Q_DECLARE_METATYPE(MIDISpecificSettings)

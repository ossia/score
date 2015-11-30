#pragma once
#include <QString>
#include <QMetaType>

struct OSCSpecificSettings
{
    int inputPort{};
    int outputPort{};
    QString host;
};
Q_DECLARE_METATYPE(OSCSpecificSettings)

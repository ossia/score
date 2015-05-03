#pragma once
#include <QString>
#include <QMetaType>

struct OSCSpecificSettings
{
    int outputPort{};
    int inputPort{};
    QString host;
};
Q_DECLARE_METATYPE(OSCSpecificSettings)

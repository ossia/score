#pragma once
#include <QString>
#include <QMetaType>

struct MinuitSpecificSettings
{
    int inputPort{};
    int outputPort{};
    QString host;
};
Q_DECLARE_METATYPE(MinuitSpecificSettings)

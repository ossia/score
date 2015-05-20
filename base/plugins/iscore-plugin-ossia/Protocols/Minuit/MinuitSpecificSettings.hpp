#pragma once
#include <QString>
#include <QMetaType>

struct MinuitSpecificSettings
{
    QString host;
    int inPort{};
    int outPort{};
};
Q_DECLARE_METATYPE(MinuitSpecificSettings)

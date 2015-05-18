#pragma once
#include <QString>
#include <QMetaType>

struct MinuitSpecificSettings
{
    int port{};
    QString host;
};
Q_DECLARE_METATYPE(MinuitSpecificSettings)

#pragma once
#include <QString>
#include <QMetaType>

namespace Ossia
{
struct OSCSpecificSettings
{
    int inputPort{};
    int outputPort{};
    QString host;
};
}

Q_DECLARE_METATYPE(Ossia::OSCSpecificSettings)

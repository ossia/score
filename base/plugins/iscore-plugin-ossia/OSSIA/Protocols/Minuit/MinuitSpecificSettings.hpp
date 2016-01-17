#pragma once
#include <QString>
#include <QMetaType>

namespace Ossia
{
struct MinuitSpecificSettings
{
    int inputPort{};
    int outputPort{};
    QString host;
};
}
Q_DECLARE_METATYPE(Ossia::MinuitSpecificSettings)

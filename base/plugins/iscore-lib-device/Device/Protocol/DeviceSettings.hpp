#pragma once
#include <QString>
#include <QVariant>
#include <iscore/tools/Todo.hpp>

namespace iscore {
struct DeviceSettings
{
        std::string protocol;
        QString name;
        QVariant deviceSpecificSettings;
};
}

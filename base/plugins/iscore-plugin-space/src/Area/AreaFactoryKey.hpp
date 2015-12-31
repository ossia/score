#pragma once
#include <QMetaType>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
namespace Space
{
class AreaTag{};
using AreaFactoryKey = StringKey<AreaTag>;
}
Q_DECLARE_METATYPE(Space::AreaFactoryKey)

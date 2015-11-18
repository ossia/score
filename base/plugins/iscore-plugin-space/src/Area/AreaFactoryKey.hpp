#pragma once
#include <QMetaType>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

class AreaTag{};
using AreaFactoryKey = StringKey<AreaTag>;
Q_DECLARE_METATYPE(AreaFactoryKey)

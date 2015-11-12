#pragma once
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <QMetaType>

class ProtocolTag{};
using ProtocolFactoryKey = StringFactoryKey<ProtocolTag>;
Q_DECLARE_METATYPE(ProtocolFactoryKey)

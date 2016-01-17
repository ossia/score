#pragma once
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <QMetaType>

namespace Device
{
class ProtocolTag{};
using ProtocolFactoryKey = StringKey<ProtocolTag>;
}
Q_DECLARE_METATYPE(Device::ProtocolFactoryKey)

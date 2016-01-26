#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <QMetaType>

namespace Device
{
class ProtocolTag{};
using ProtocolFactoryKey = UuidKey<ProtocolTag>;
}
Q_DECLARE_METATYPE(Device::ProtocolFactoryKey)

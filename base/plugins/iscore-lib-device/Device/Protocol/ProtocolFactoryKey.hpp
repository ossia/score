#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <QMetaType>

namespace Device
{
class DeviceInterface;
using ProtocolFactoryKey = UuidKey<DeviceInterface>;
}
Q_DECLARE_METATYPE(Device::ProtocolFactoryKey)

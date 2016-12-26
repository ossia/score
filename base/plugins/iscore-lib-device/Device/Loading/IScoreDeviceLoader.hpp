#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <QString>

namespace Device
{
ISCORE_LIB_DEVICE_EXPORT bool
loadDeviceFromIScoreJSON(const QString& filePath, Device::Node& node);
}

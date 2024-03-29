#pragma once
#include <Device/Node/DeviceNode.hpp>

#include <QString>

namespace Device
{
SCORE_LIB_DEVICE_EXPORT bool
loadDeviceFromTouchOSC(const QString& filePath, Device::Node& node);
}

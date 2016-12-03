#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <QString>

namespace Device
{
// Will load a device in a Jamoma-format XML file into the node.
// The node has to be the device node.
ISCORE_LIB_DEVICE_EXPORT bool
loadDeviceFromXML(const QString& filePath, Device::Node& node);
ISCORE_LIB_DEVICE_EXPORT bool
loadDeviceFromJSON(const QString& filePath, Device::Node& node);
}

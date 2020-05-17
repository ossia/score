#pragma once
#include <Device/Node/DeviceNode.hpp>

#include <QString>

namespace Device
{
/**
 * @brief loadDeviceFromJamomaJSON Will load a device in a Jamoma-format XML
 * file into the node.
 *
 * The node has to be the device node.
 */
SCORE_LIB_DEVICE_EXPORT bool loadDeviceFromXML(const QString& filePath, Device::Node& node);

/**
 * @brief loadDeviceFromJamomaJSON Will load a device in a Jamoma-format JSON
 * file into the node.
 *
 * The node has to be the device node.
 */
SCORE_LIB_DEVICE_EXPORT bool loadDeviceFromJamomaJSON(const QString& filePath, Device::Node& node);
}

#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <QString>

// Will load a device in a Jamoma-format XML file into the node.
// The node has to be the device node.
void loadDeviceFromXML(const QString& filePath, iscore::Node& node);

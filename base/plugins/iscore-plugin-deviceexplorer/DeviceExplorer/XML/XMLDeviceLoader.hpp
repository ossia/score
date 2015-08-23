#pragma once
#include <QString>
#include <DeviceExplorer/Node/Node.hpp>

// Will load a device in a Jamoma-format XML file into the node.
// The node has to be the device node.
void loadDeviceFromXML(const QString& filePath, iscore::Node& node);

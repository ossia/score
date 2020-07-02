#pragma once
#include <Device/Node/DeviceNode.hpp>

#include <QString>

namespace Device
{
SCORE_LIB_DEVICE_EXPORT bool loadDeviceFromScoreJSON(const QString& filePath, Device::Node& node);
SCORE_LIB_DEVICE_EXPORT bool loadDeviceFromScoreJSON(const rapidjson::Document& jsonContent, Device::Node& node);
}

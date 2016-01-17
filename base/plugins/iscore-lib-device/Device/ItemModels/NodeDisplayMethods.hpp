#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <QVariant>

namespace Device
{
class DeviceInterface;
}
/**
 * These functions allow to display
 * the informations in Device::Node's
 * friendly in QAbstractItemModel derivatives.
 */
namespace Device
{
ISCORE_LIB_DEVICE_EXPORT QVariant nameColumnData(const Device::Node& node, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant deviceNameColumnData(const Device::Node& node, Device::DeviceInterface& dev, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant valueColumnData(const Device::Node& node, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant GetColumnData(const Device::Node& node, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant SetColumnData(const Device::Node& node, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant minColumnData(const Device::Node& node, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant maxColumnData(const Device::Node& node, int role);
}

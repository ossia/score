#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <QVariant>

class DeviceInterface;

/**
 * These functions allow to display
 * the informations in iscore::Node's
 * friendly in QAbstractItemModel derivatives.
 */
namespace DeviceExplorer
{
ISCORE_LIB_DEVICE_EXPORT QVariant nameColumnData(const iscore::Node& node, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant deviceNameColumnData(const iscore::Node& node, DeviceInterface& dev, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant valueColumnData(const iscore::Node& node, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant GetColumnData(const iscore::Node& node, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant SetColumnData(const iscore::Node& node, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant minColumnData(const iscore::Node& node, int role);
ISCORE_LIB_DEVICE_EXPORT QVariant maxColumnData(const iscore::Node& node, int role);
}

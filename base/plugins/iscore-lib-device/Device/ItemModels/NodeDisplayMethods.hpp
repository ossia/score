#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <qvariant.h>

class DeviceInterface;

/**
 * These functions allow to display
 * the informations in iscore::Node's
 * friendly in QAbstractItemModel derivatives.
 */
namespace DeviceExplorer
{
QVariant nameColumnData(const iscore::Node& node, int role);
QVariant deviceNameColumnData(const iscore::Node& node, DeviceInterface& dev, int role);
QVariant valueColumnData(const iscore::Node& node, int role);
QVariant GetColumnData(const iscore::Node& node, int role);
QVariant SetColumnData(const iscore::Node& node, int role);
QVariant minColumnData(const iscore::Node& node, int role);
QVariant maxColumnData(const iscore::Node& node, int role);
}

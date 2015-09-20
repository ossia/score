#pragma once
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

namespace iscore
{
/**
 * These functions allow to display
 * the informations in iscore::Node's
 * friendly in QAbstractItemModel derivatives.
 */

QVariant nameColumnData(const Node& node, int role);
QVariant valueColumnData(const Node& node, int role);
QVariant IOTypeColumnData(const Node& node, int role);
QVariant minColumnData(const Node& node, int role);
QVariant maxColumnData(const Node& node, int role);
}

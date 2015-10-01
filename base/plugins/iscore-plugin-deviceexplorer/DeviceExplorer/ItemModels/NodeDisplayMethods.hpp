#pragma once
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

/**
 * These functions allow to display
 * the informations in iscore::Node's
 * friendly in QAbstractItemModel derivatives.
 */
namespace DeviceExplorer
{
QVariant nameColumnData(const iscore::Node& node, int role);
QVariant valueColumnData(const iscore::Node& node, int role);
QVariant IOTypeColumnData(const iscore::Node& node, int role);
QVariant minColumnData(const iscore::Node& node, int role);
QVariant maxColumnData(const iscore::Node& node, int role);
}

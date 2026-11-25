#pragma once
#include <QPointF>
#include <QSizeF>

#include <score_plugin_scenario_export.h>

#include <vector>

namespace Scenario
{
//! A cable from node `from` to node `to`, leaving and entering at these
//! heights below the nodes' tops.
struct LayoutEdge
{
  int from{}, to{};
  double fromY{}, toY{};
};

//! The top-left corners of nodes of these sizes in a layered (Sugiyama)
//! layout, edges flowing left to right; the first node at (0, 0) or below.
//! Connected nodes are laid out per component, components stacked by
//! decreasing size; unconnected nodes go in rows below them. Deterministic:
//! the result depends on the order of the nodes and edges only.
SCORE_PLUGIN_SCENARIO_EXPORT
std::vector<QPointF>
layeredLayout(const std::vector<QSizeF>& sizes, const std::vector<LayoutEdge>& edges);
}

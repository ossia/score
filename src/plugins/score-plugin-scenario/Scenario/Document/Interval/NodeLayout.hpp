#pragma once
#include <QPointF>
#include <QRectF>

#include <score_plugin_scenario_export.h>

#include <functional>
#include <optional>
#include <vector>

class QMenu;
namespace Process
{
class ProcessModel;
class Port;
}
namespace score
{
struct DocumentContext;
}

namespace Scenario
{
class IntervalModel;

struct NodePlacement
{
  const Process::ProcessModel* process{};
  QPointF position;
};

//! What a view knows of the nodes it draws, both relative to the node's
//! position; without it the layout estimates them from the model.
struct NodeGeometry
{
  std::function<std::optional<QRectF>(const Process::ProcessModel&)> bounds;
  std::function<std::optional<double>(const Process::Port&)> portY;
};

//! Layered (Sugiyama) layout of the nodes of an interval, cables flowing left
//! to right. `nodes` empty means every process of the interval; otherwise only
//! those are moved, and they keep the top-left corner they occupy.
SCORE_PLUGIN_SCENARIO_EXPORT
std::vector<NodePlacement> layoutNodes(
    const IntervalModel& interval, std::vector<const Process::ProcessModel*> nodes,
    const score::DocumentContext& ctx, const NodeGeometry& geometry = {});

//! layoutNodes, applied as one undoable command.
SCORE_PLUGIN_SCENARIO_EXPORT
void autoLayoutNodes(
    const IntervalModel& interval, std::vector<const Process::ProcessModel*> nodes,
    const score::DocumentContext& ctx, const NodeGeometry& geometry = {});

//! "Auto-layout" entries of the nodal views' context menus. `laidOutAll` runs
//! after every node of the interval has been placed.
SCORE_PLUGIN_SCENARIO_EXPORT
void addNodeLayoutActions(
    QMenu& menu, const IntervalModel& interval, const score::DocumentContext& ctx,
    NodeGeometry geometry = {}, std::function<void()> laidOutAll = {});
}

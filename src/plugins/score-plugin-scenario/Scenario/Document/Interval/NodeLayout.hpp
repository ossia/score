#pragma once
#include <QPointF>
#include <QRectF>

#include <score_plugin_scenario_export.h>

#include <functional>
#include <optional>
#include <vector>

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

//! The bounds a NodeItem draws a process with, relative to its position,
//! estimated from the model alone.
SCORE_PLUGIN_SCENARIO_EXPORT
QRectF estimatedNodeBounds(const Process::ProcessModel& process);

//! Layered layout (layeredLayout) of the nodes of an interval, cables flowing
//! left to right. `nodes` empty means every process of the interval;
//! otherwise only those of them that belong to it are moved, keeping the
//! top-left corner they occupy.
SCORE_PLUGIN_SCENARIO_EXPORT
std::vector<NodePlacement> layoutNodes(
    const IntervalModel& interval, std::vector<const Process::ProcessModel*> nodes,
    const score::DocumentContext& ctx, const NodeGeometry& geometry = {});

//! layoutNodes, applied as one undoable command; nothing when no node moves.
//! Whether anything moved.
SCORE_PLUGIN_SCENARIO_EXPORT
bool autoLayoutNodes(
    const IntervalModel& interval, std::vector<const Process::ProcessModel*> nodes,
    const score::DocumentContext& ctx, const NodeGeometry& geometry = {});
}

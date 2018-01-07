#pragma once
#include <Process/Dataflow/Cable.hpp>
#include <score_plugin_scenario_export.h>

namespace Dataflow
{
using SerializedCables = QVector<QPair<Id<Process::Cable>, Process::CableData>>;


SCORE_PLUGIN_SCENARIO_EXPORT
SerializedCables saveCables(QObjectList objs, const score::DocumentContext& ctx);

SCORE_PLUGIN_SCENARIO_EXPORT
void removeCables(const SerializedCables& cbls, const score::DocumentContext& ctx);

SCORE_PLUGIN_SCENARIO_EXPORT
void restoreCables(const SerializedCables& cbls, const score::DocumentContext& ctx);

SCORE_PLUGIN_SCENARIO_EXPORT
std::vector<Process::Cable*> getCablesInChildObjects(QObjectList objs, const score::DocumentContext& ctx);

}

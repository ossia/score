#pragma once
#include <Process/Dataflow/Cable.hpp>

#include <score_plugin_scenario_export.h>

//! MOVEME process
namespace Process
{
class ProcessModel;
}
namespace Dataflow
{
using SerializedCables = std::vector<std::pair<Id<Process::Cable>, Process::CableData>>;

//! Get the cables connected between objects
SCORE_PLUGIN_SCENARIO_EXPORT
std::vector<Process::Cable*>
getCablesInChildObjects(QObjectList objs, const score::DocumentContext& ctx);

//! Saves cables connected to objects in objs
SCORE_PLUGIN_SCENARIO_EXPORT
SerializedCables saveCables(QObjectList objs, const score::DocumentContext& ctx);

//! Remove cables in that set
SCORE_PLUGIN_SCENARIO_EXPORT
void removeCables(const SerializedCables& cbls, const score::DocumentContext& ctx);

//! Given the same document, restore cables that were removed - mostly useful
//! for undo operations.
SCORE_PLUGIN_SCENARIO_EXPORT
void restoreCables(const SerializedCables& cbls, const score::DocumentContext& ctx);

//! Restore cables. The objects must exist but it can be in a different
//! document, at a different place in the hierarchy.
SCORE_PLUGIN_SCENARIO_EXPORT
void loadCables(
    const ObjectPath& old_path,
    const ObjectPath& new_path,
    Dataflow::SerializedCables& cables,
    const score::DocumentContext& ctx);
}

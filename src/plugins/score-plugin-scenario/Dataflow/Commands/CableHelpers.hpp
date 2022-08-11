#pragma once
#include <Process/Dataflow/Cable.hpp>

#include <ossia/detail/json.hpp>

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

//! Converts a cable array loaded directly from a .score to a SerializedCables array,
//! used for dropping a .score in another
SCORE_PLUGIN_SCENARIO_EXPORT
SerializedCables serializedCablesFromCableJson(
    const ObjectPath& old_path, const ObjectPath& new_path,
    const rapidjson::Document::Array& arr);
SCORE_PLUGIN_SCENARIO_EXPORT
SerializedCables serializedCablesFromCableJson(
    const ObjectPath& old_path, const rapidjson::Document::Array& arr);

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
    const ObjectPath& old_path, const ObjectPath& new_path,
    Dataflow::SerializedCables& cables, const score::DocumentContext& ctx);

//! Add a prefix to a set of cables.
//! This case is for when the cables's "prefix" path is empty.
//! This is used for instance in LoadCables, ReplaceAllNodes, when dropping
//! presets...
SCORE_PLUGIN_SCENARIO_EXPORT
void unstripCables(const ObjectPath& new_path, Dataflow::SerializedCables& cables);
}

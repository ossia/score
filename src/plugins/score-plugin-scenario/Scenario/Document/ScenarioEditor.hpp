#pragma once
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <score/model/ObjectEditor.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <score_plugin_scenario_export.h>

#include <vector>
namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class ProcessModel;

namespace Command
{
class Macro;
}
class IntervalModel;

//! Loads copied processes in the interval, moving anchors between them to the
//! copies; fills the old to new id mapping and returns the copies' paths.
SCORE_PLUGIN_SCENARIO_EXPORT
std::vector<ObjectPath> loadCopiedProcesses(
    Command::Macro& m, const IntervalModel& interval, const rapidjson::Value& copy,
    std::vector<std::pair<int32_t, int32_t>>& proc_id_map,
    const score::DocumentContext& ctx);

//! Copies the process in its interval; references to its own objects move to the copy's
SCORE_PLUGIN_SCENARIO_EXPORT
void duplicateProcess(
    const IntervalModel& interval, const Process::ProcessModel& proc,
    const score::DocumentContext& ctx);

//! Pastes processes copied by copySelectedProcesses in a new interval at origin;
//! startsOnPlay gives its start the trigger a double-click gives
SCORE_PLUGIN_SCENARIO_EXPORT
bool pasteProcessesInNewBox(
    const Scenario::ProcessModel& sm, Scenario::Point origin, rapidjson::Value& obj,
    const score::DocumentContext& ctx, bool startsOnPlay = false);

using score::RemovalLinks;

//! Removes processes of intervals, in one undo step.
//!
//! A process sits in a chain through its first inlet and first outlet when
//! they carry the same kind of data. With Reconnect, the removed processes
//! that follow each other in a chain go as one sub-chain: what fed its first
//! process feeds what its last one fed, without adding a cable that already
//! exists, and a sub-chain fed by nothing gives the address of its first inlet
//! to what it fed. With Disconnect, their cables go with them.
//!
//! In both modes, the tail of a chain, a removed process whose chain outlet has
//! no cable, hands the propagation of its audio outlet back to every outlet
//! that fed its sub-chain and is left with no cable: each of them was heard
//! through it, and one that still feeds something stays heard there. With
//! Reconnect, the address of the tail goes back to them as well; it does not
//! with Disconnect, where a raw source must not start writing to the address
//! a mapping or a filter was writing to.
SCORE_PLUGIN_SCENARIO_EXPORT
void removeIntervalProcesses(
    const std::vector<const Process::ProcessModel*>& procs, RemovalLinks links,
    const score::DocumentContext& ctx);

class ScenarioEditor final : public score::ObjectEditor
{
  SCORE_CONCRETE("90265073-0aae-4628-834a-f44048664476")

  bool
  copy(JSONReader& r, const Selection& s, const score::DocumentContext& ctx) override;
  bool paste(
      QPoint pos, QObject* focusedObject, const QMimeData& mime,
      const score::DocumentContext& ctx) override;
  bool remove(
      const Selection& s, const score::DocumentContext& ctx,
      RemovalLinks links) override;
};

}

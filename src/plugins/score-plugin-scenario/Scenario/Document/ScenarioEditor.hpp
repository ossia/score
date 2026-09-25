#pragma once
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <score/model/ObjectEditor.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <score_plugin_scenario_export.h>
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

//! Pastes processes copied by copySelectedProcesses in a new interval at origin
SCORE_PLUGIN_SCENARIO_EXPORT
bool pasteProcessesInNewBox(
    const Scenario::ProcessModel& sm, Scenario::Point origin, rapidjson::Value& obj,
    const score::DocumentContext& ctx);

class ScenarioEditor final : public score::ObjectEditor
{
  SCORE_CONCRETE("90265073-0aae-4628-834a-f44048664476")

  bool
  copy(JSONReader& r, const Selection& s, const score::DocumentContext& ctx) override;
  bool paste(
      QPoint pos, QObject* focusedObject, const QMimeData& mime,
      const score::DocumentContext& ctx) override;
  bool remove(const Selection& s, const score::DocumentContext& ctx) override;
};

}

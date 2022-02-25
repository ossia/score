#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>

#include <Process/ExpandMode.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/HashMap.hpp>

#include <ossia/detail/json.hpp>

#include <QPointF>

struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class IntervalModel;

namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT PasteProcessesInInterval final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      PasteProcessesInInterval,
      "Paste processes in a interval")
public:
  PasteProcessesInInterval(
      rapidjson::Value::Array sourceProcesses,
      rapidjson::Value::Array sourceCables,
      const IntervalModel& targetInterval,
      ExpandMode mode,
      QPointF origin);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<IntervalModel> m_target;
  ExpandMode m_mode{ExpandMode::GrowShrink};
  QPointF m_origin{};

  std::vector<Id<Process::ProcessModel>> m_ids_processes;
  std::vector<QByteArray> m_json_processes;

  CopiedCables m_cables;
};
}
}

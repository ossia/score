#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

#include <score_plugin_scenario_export.h>
namespace Process
{
class ProcessModel;
class ProcessFactory;
}
namespace Scenario
{
class StateModel;
namespace Command
{

class SCORE_PLUGIN_SCENARIO_EXPORT RemoveStateProcess final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveStateProcess, "Remove a state process")
public:
  RemoveStateProcess(const Scenario::StateModel& state, Id<Process::ProcessModel> processId);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<StateModel> m_path;
  UuidKey<Process::ProcessModel> m_processUuid;

  Id<Process::ProcessModel> m_processId{};
  QByteArray m_data;
};
}
}

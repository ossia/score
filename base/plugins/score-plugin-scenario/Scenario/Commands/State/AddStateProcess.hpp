#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/Identifier.hpp>

#include <score_plugin_scenario_export.h>

// RENAMEME

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
class SCORE_PLUGIN_SCENARIO_EXPORT AddStateProcessToState final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      AddStateProcessToState,
      "Add a state process")
public:
  AddStateProcessToState(
      const Scenario::StateModel& state,
      UuidKey<Process::ProcessModel> process);
  AddStateProcessToState(
      const Scenario::StateModel& state,
      Id<Process::ProcessModel> idToUse,
      UuidKey<Process::ProcessModel> process);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<StateModel> m_path;
  UuidKey<Process::ProcessModel> m_processName;
  QString m_data{};
  Id<Process::ProcessModel> m_createdProcessId{};
};
}
}

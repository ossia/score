#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/Identifier.hpp>

#include <score_plugin_scenario_export.h>

// RENAMEME

namespace Process
{
class StateProcess;
class StateProcessFactory;
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
      UuidKey<Process::StateProcessFactory> process);
  AddStateProcessToState(
      const Scenario::StateModel& state,
      Id<Process::StateProcess> idToUse,
      UuidKey<Process::StateProcessFactory> process);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<StateModel> m_path;
  UuidKey<Process::StateProcessFactory> m_processName;

  Id<Process::StateProcess> m_createdProcessId{};
};
}
}

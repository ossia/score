#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/Identifier.hpp>

#include <score_plugin_scenario_export.h>
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

class SCORE_PLUGIN_SCENARIO_EXPORT RemoveStateProcess final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      RemoveStateProcess,
      "Remove a state process")
public:
  RemoveStateProcess(
      const Scenario::StateModel& state,
      Id<Process::StateProcess> processId);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<StateModel> m_path;
  UuidKey<Process::StateProcessFactory> m_processUuid;

  Id<Process::StateProcess> m_processId{};
};
}
}

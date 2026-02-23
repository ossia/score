#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/Script/ScriptEditor.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Dataflow/Commands/CableHelpers.hpp>

#include <score/tools/Unused.hpp>

#include <ossia/detail/algorithms.hpp>

#include <score_plugin_scenario_export.h>
namespace Scenario
{
// TODO find a way to keep this in sync with Scenario::EditScript
class SCORE_PLUGIN_SCENARIO_EXPORT SetControllerControlValue : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), SetControllerControlValue,
      "Set a control")
public:
  SetControllerControlValue(
      const Process::ControlInlet& obj, ossia::value newval,
      const score::DocumentContext& ctx);

  virtual ~SetControllerControlValue();

  void undo(const score::DocumentContext& ctx) const final override;
  void redo(const score::DocumentContext& ctx) const final override;

  void update(const Process::ControlInlet& obj, ossia::value newval, unused_t);

protected:
  void serializeImpl(DataStreamInput& stream) const final override;
  void deserializeImpl(DataStreamOutput& stream) final override;

private:
  Path<Process::ControlInlet> m_path;
  ossia::value m_old, m_new;
  std::vector<Dataflow::SavedPort> m_oldInlets, m_oldOutlets;

  Dataflow::SerializedCables m_oldCables;
};
}

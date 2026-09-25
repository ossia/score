#pragma once
#include <Process/Commands/LoadPresetCommandFactory.hpp>
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
class SCORE_PLUGIN_SCENARIO_EXPORT SetControllerControlValue
    : public Process::ChangePortsCommand
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
  void setNewValue(ossia::value v) override { m_new = std::move(v); }

protected:
  void serializeImpl(DataStreamInput& stream) const final override;
  void deserializeImpl(DataStreamOutput& stream) final override;

private:
  Path<Process::ControlInlet> m_path;
  ossia::value m_old, m_new;
  std::vector<Dataflow::SavedPort> m_oldInlets, m_oldOutlets;

  Dataflow::SerializedCables m_oldCables;
};

//! Sets controls that change the ports of their process with SetControllerControlValue
class ChangePortsCommandFactory final : public Process::ChangePortsCommandFactory
{
  SCORE_CONCRETE("8a3e1f65-3c4b-4d0e-a0a1-2a8c7c3e5b7d")
public:
  bool matches(
      const Process::ControlInlet&, unused_t, const score::DocumentContext&) const noexcept override
  {
    return true;
  }
  Process::ChangePortsCommand* make(
      const Process::ControlInlet& obj, ossia::value newval,
      const score::DocumentContext& ctx) const override
  {
    return new SetControllerControlValue{obj, std::move(newval), ctx};
  }
};
}

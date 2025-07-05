#pragma once
#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Dataflow/Commands/CableHelpers.hpp>

#include <score/model/path/PathSerialization.hpp>

#include <score_plugin_scenario_export.h>

namespace Scenario::Command
{
class LoadPreset final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), LoadPreset, "Load a preset")

public:
  LoadPreset(const Process::ProcessModel& obj, Process::Preset newval)
      : m_path{obj}
      , m_old{obj.savePreset()}
      , m_new{std::move(newval)}
  {
  }

private:
  void undo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).loadPreset(m_old);
  }

  void redo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).loadPreset(m_new);
  }

  void serializeImpl(DataStreamInput& stream) const final override
  {
    stream << m_path << m_old << m_new;
  }
  void deserializeImpl(DataStreamOutput& stream) final override
  {
    stream >> m_path >> m_old >> m_new;
  }

  Path<Process::ProcessModel> m_path;
  Process::Preset m_old;
  Process::Preset m_new;
};

// Note: see ScriptEditCommand.hpp which is very similar!
class LoadPresetWithCablesBackup final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), LoadPresetWithCablesBackup,
      "Load a preset")

public:
  LoadPresetWithCablesBackup(
      const Process::ProcessModel& model, Process::Preset newval,
      const score::DocumentContext& ctx)
      : m_path{model}
      , m_old{model.savePreset()}
      , m_new{std::move(newval)}
  {
    m_oldCables
        = Dataflow::saveCables({const_cast<Process::ProcessModel*>(&model)}, ctx);

    for(auto& port : model.inlets())
      m_oldInlets.emplace_back(
          Dataflow::SavedPort{port->name(), port->type(), port->saveData()});
    for(auto& port : model.outlets())
      m_oldOutlets.emplace_back(
          Dataflow::SavedPort{port->name(), port->type(), port->saveData()});
  }

private:
  void undo(const score::DocumentContext& ctx) const final override
  {
    auto& cmt = m_path.find(ctx);
    // Remove all the cables that could have been added during
    // the creation
    Dataflow::removeCables(m_oldCables, ctx);

    // Set the old preset
    cmt.loadPreset(m_old);

    // We expect the inputs / outputs to revert back to the
    // exact same state
    SCORE_ASSERT(m_oldInlets.size() == cmt.inlets().size());
    SCORE_ASSERT(m_oldOutlets.size() == cmt.outlets().size());

    // So we can reload their data identically
    for(std::size_t i = 0; i < m_oldInlets.size(); i++)
    {
      cmt.inlets()[i]->loadData(m_oldInlets[i].data);
    }
    for(std::size_t i = 0; i < m_oldOutlets.size(); i++)
    {
      cmt.outlets()[i]->loadData(m_oldOutlets[i].data);
    }

    // Recreate the old cables
    Dataflow::restoreCables(m_oldCables, ctx);
    cmt.inletsChanged();
    cmt.outletsChanged();
  }

  void redo(const score::DocumentContext& ctx) const final override
  {
    Dataflow::removeCables(m_oldCables, ctx);

    auto& cmt = m_path.find(ctx);
    cmt.loadPreset(m_new);

    Dataflow::reloadPortsInNewProcess(m_oldInlets, m_oldOutlets, m_oldCables, cmt, ctx);

    cmt.inletsChanged();
    cmt.outletsChanged();
    // FIXME if we have it only here, then changing cables fails for the exec nodes
    // as in the cable loading, in SetupContext::connectCable(Process::Cable& cable)
    // auto it = outlets.find(port_src); fails because the new outlet hasn't yet been created by the component
    // but if we have it only above, the JS GPU node fails
  }

  void serializeImpl(DataStreamInput& stream) const final override
  {
    stream << m_path << m_old << m_new << m_oldInlets << m_oldOutlets << m_oldCables;
  }
  void deserializeImpl(DataStreamOutput& stream) final override
  {
    stream >> m_path >> m_old >> m_new >> m_oldInlets >> m_oldOutlets >> m_oldCables;
  }

  Path<Process::ProcessModel> m_path;
  Process::Preset m_old;
  Process::Preset m_new;

  std::vector<Dataflow::SavedPort> m_oldInlets, m_oldOutlets;

  Dataflow::SerializedCables m_oldCables;
};

class LoadPresetCommandFactory final : public Process::LoadPresetCommandFactory
{
  SCORE_CONCRETE("b04a9d8a-1374-4223-a258-0f90e8e0aef6")
public:
  bool matches(
      const Process::ProcessModel& obj, unused_t,
      const score::DocumentContext& ctx) const noexcept override
  {
    return true;
  }
  score::Command* make(
      const Process::ProcessModel& obj, Process::Preset newval,
      const score::DocumentContext& ctx) const override
  {
    if(obj.flags() & Process::ProcessFlags::DynamicPorts)
      return new LoadPresetWithCablesBackup{obj, std::move(newval), ctx};
    else
      return new LoadPreset{obj, std::move(newval)};
  }
};
}

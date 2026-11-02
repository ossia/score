#pragma once
#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Dataflow/Commands/CableHelpers.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QDebug>

#include <algorithm>
#include <vector>

#include <score_plugin_scenario_export.h>

namespace Scenario::Command
{
//! The ids and types of a process's inlets and outlets, in order
struct PortSignature
{
  std::vector<std::pair<int, Process::PortType>> inlets, outlets;
  bool operator==(const PortSignature&) const noexcept = default;

  //! Every port of `before` is still there, in its place, of its type: the
  //! cables on them are still valid. A plug-in may expose more controls.
  bool keeps(const PortSignature& before) const noexcept
  {
    const auto prefix = [](const auto& now, const auto& old) {
      return now.size() >= old.size() && std::equal(old.begin(), old.end(), now.begin());
    };
    return prefix(inlets, before.inlets) && prefix(outlets, before.outlets);
  }
};

inline PortSignature portSignature(const Process::ProcessModel& proc)
{
  PortSignature sig;
  for(auto* p : proc.inlets())
    sig.inlets.emplace_back(p->id().val(), p->type());
  for(auto* p : proc.outlets())
    sig.outlets.emplace_back(p->id().val(), p->type());
  return sig;
}

//! For a process whose presets do not change its ports: only the values
//! change, the cables stay where they are
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
    load(m_path.find(ctx), m_old);
  }

  void redo(const score::DocumentContext& ctx) const final override
  {
    load(m_path.find(ctx), m_new);
  }

  static void load(Process::ProcessModel& proc, const Process::Preset& p)
  {
    const auto before = portSignature(proc);
    proc.loadPreset(p);
    if(!portSignature(proc).keeps(before))
    {
      // Its cables may now be on ports of another type
      qWarning() << "Loading a preset changed the ports of" << proc.prettyName()
                 << ": its presetChangesPorts() must say so";
      SCORE_SOFT_ASSERT(false);
    }
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
class SCORE_PLUGIN_SCENARIO_TEST_EXPORT LoadPresetWithCablesBackup final : public score::Command
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

    // Recreate the old cables; the ports at the other end lost them too
    auto cables = Dataflow::restoreCablesWithoutTouchingPorts(m_oldCables, ctx);
    Dataflow::reattachCablesToPorts(cables, ctx);
    cmt.inletsChanged();
    cmt.outletsChanged();
    Dataflow::notifyAddedCables(cables, ctx);
  }

  void redo(const score::DocumentContext& ctx) const final override
  {
    Dataflow::removeCables(m_oldCables, ctx);

    auto& cmt = m_path.find(ctx);
    std::vector<int> inletIds, outletIds;
    inletIds.reserve(cmt.inlets().size());
    outletIds.reserve(cmt.outlets().size());
    for(auto* port : cmt.inlets())
      inletIds.push_back(port->id().val());
    for(auto* port : cmt.outlets())
      outletIds.push_back(port->id().val());
    cmt.loadPreset(m_new);

    // Cables and addresses only: the values are the preset's now, and a
    // controller given its old value back would resize the ports again.
    auto cables = Dataflow::reloadPortsInNewProcess(
        m_oldInlets, m_oldOutlets, m_oldCables, cmt, Process::PortLoadDataFlags{}, ctx,
        inletIds, outletIds);

    cmt.inletsChanged();
    cmt.outletsChanged();
    Dataflow::notifyAddedCables(cables, ctx);
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
    // Cables are taken off and matched again only around a load that can
    // move the ports they are on
    if(obj.presetChangesPorts(newval))
      return new LoadPresetWithCablesBackup{obj, std::move(newval), ctx};
    else
      return new LoadPreset{obj, std::move(newval)};
  }
};
}

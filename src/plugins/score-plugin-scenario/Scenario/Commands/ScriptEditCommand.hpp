#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/Script/ScriptEditor.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <Dataflow/Commands/CableHelpers.hpp>

#include <ossia/detail/algorithms.hpp>
namespace Scenario
{
using SavedPort = Dataflow::SavedPort;

template <typename Process_T, typename Property_T>
class EditScript : public score::Command
{
public:
  using param_type = typename Property_T::param_type;
  using score::Command::Command;
  EditScript(
      const Process_T& model, param_type newScript, const score::DocumentContext& ctx)
      : m_path{model}
      , m_newScript{std::move(newScript)}
      , m_oldScript{(model.*Property_T::get)()}
  {
    m_oldCables = Dataflow::saveCables({const_cast<Process_T*>(&model)}, ctx);

    for(auto& port : model.inlets())
      m_oldInlets.emplace_back(
          Dataflow::SavedPort{port->name(), port->type(), port->saveData()});
    for(auto& port : model.outlets())
      m_oldOutlets.emplace_back(
          Dataflow::SavedPort{port->name(), port->type(), port->saveData()});
  }

private:
  void undo(const score::DocumentContext& ctx) const override
  {
    auto& cmt = m_path.find(ctx);
    // Remove all the cables that could have been added during
    // the creation
    Dataflow::removeCables(m_oldCables, ctx);

    // Set the old script
    Process::ScriptChangeResult res = (cmt.*Property_T::set)(m_oldScript);
    cmt.programChanged();

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
    auto cables = Dataflow::restoreCablesWithoutTouchingPorts(m_oldCables, ctx);
    cmt.inletsChanged();
    cmt.outletsChanged();

    if constexpr(requires { cmt.isGpu(); })
      if(cmt.isGpu())
        cmt.programChanged();

    Dataflow::notifyAddedCables(cables, ctx);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    Dataflow::removeCables(m_oldCables, ctx);

    auto& cmt = m_path.find(ctx);
    Process::ScriptChangeResult res = (cmt.*Property_T::set)(m_newScript);
    cmt.programChanged();

    auto cables = Dataflow::reloadPortsInNewProcess(
        m_oldInlets, m_oldOutlets, m_oldCables, cmt, ctx);

    cmt.inletsChanged();
    cmt.outletsChanged();

    if constexpr(requires { cmt.isGpu(); })
      if(cmt.isGpu())
        cmt.programChanged();

    Dataflow::notifyAddedCables(cables, ctx);
  }

  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_newScript << m_oldScript << m_oldInlets << m_oldOutlets
      << m_oldCables;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_path >> m_newScript >> m_oldScript >> m_oldInlets >> m_oldOutlets
        >> m_oldCables;
  }

  Path<Process_T> m_path;
  param_type m_newScript;
  param_type m_oldScript;

  std::vector<SavedPort> m_oldInlets, m_oldOutlets;

  Dataflow::SerializedCables m_oldCables;
};
}

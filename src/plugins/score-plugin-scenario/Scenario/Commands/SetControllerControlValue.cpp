#include "SetControllerControlValue.hpp"

#include <QApplication>
namespace Scenario
{

SetControllerControlValue::SetControllerControlValue(
    const Process::ControlInlet& obj, ossia::value newval,
    const score::DocumentContext& ctx)
    : m_path{obj}
    , m_old{obj.value()}
    , m_new{newval}
{
  auto& model = *safe_cast<Process::ProcessModel*>(obj.parent());
  m_oldCables = Dataflow::saveCables({const_cast<Process::ProcessModel*>(&model)}, ctx);

  for(auto& port : model.inlets())
    m_oldInlets.emplace_back(
        Dataflow::SavedPort{port->name(), port->type(), port->saveData()});
  for(auto& port : model.outlets())
    m_oldOutlets.emplace_back(
        Dataflow::SavedPort{port->name(), port->type(), port->saveData()});
}

SetControllerControlValue::~SetControllerControlValue() { }

void SetControllerControlValue::undo(const score::DocumentContext& ctx) const
{
  const auto& cmt = m_path.find(ctx);
  auto& proc = *safe_cast<Process::ProcessModel*>(cmt.parent());
  // Remove all the cables that could have been added during
  // the creation
  Dataflow::removeCables(m_oldCables, ctx);

  m_path.find(ctx).setValue(m_old);

  // FIXME prevent valueChanged to adjust the port count here

  // We expect the inputs / outputs to revert back to the
  // exact same state
  SCORE_ASSERT(m_oldInlets.size() == proc.inlets().size());
  SCORE_ASSERT(m_oldOutlets.size() == proc.outlets().size());

  // So we can reload their data identically
  for(std::size_t i = 0; i < m_oldInlets.size(); i++)
  {
    proc.inlets()[i]->loadData(m_oldInlets[i].data);
  }
  for(std::size_t i = 0; i < m_oldOutlets.size(); i++)
  {
    proc.outlets()[i]->loadData(m_oldOutlets[i].data);
  }

  // Recreate the old cables
  auto cables = Dataflow::restoreCablesWithoutTouchingPorts(m_oldCables, ctx);

  proc.inletsChanged();
  proc.outletsChanged();
  if(ctx.document.loaded())
  {
    QTimer::singleShot(30, &proc, [cables, &proc, &ctx]() mutable {
      Dataflow::notifyAddedCables(cables, ctx);
    });
  }
  else
  {
    Dataflow::notifyAddedCables(cables, ctx);
  }
}

void SetControllerControlValue::redo(const score::DocumentContext& ctx) const
{
  if(auto obj = m_path.try_find(ctx))
  {
    Dataflow::removeCables(m_oldCables, ctx);

    obj->setValue(m_new);

    auto& proc = *safe_cast<Process::ProcessModel*>(obj->parent());
    auto cables = Dataflow::reloadPortsInNewProcess(
        m_oldInlets, m_oldOutlets, m_oldCables, proc, Process::PortLoadDataFlags{}, ctx);

    if(ctx.document.loaded())
    {
      // FIXME instead havee a kind of nursery
      // in the exec thread, so that cables for missing inlets can get a chance to be found later on?
      QTimer::singleShot(30, &proc, [cables, &proc, &ctx]() mutable {
        Dataflow::notifyAddedCables(cables, ctx);
      });
    }
    else
    {
      Dataflow::notifyAddedCables(cables, ctx);
    }
  }
  else
  {
    qDebug() << "Could not find: " << m_path.unsafePath().toString();
  }
}

void SetControllerControlValue::update(
    const Process::ControlInlet& obj, ossia::value newval, unused_t)
{
  m_new = std::move(newval);
}

void SetControllerControlValue::serializeImpl(DataStreamInput& stream) const
{
  stream << m_path << m_old << m_new << m_oldInlets << m_oldOutlets << m_oldCables;
}
void SetControllerControlValue::deserializeImpl(DataStreamOutput& stream)
{
  stream >> m_path >> m_old >> m_new >> m_oldInlets >> m_oldOutlets >> m_oldCables;
}
}

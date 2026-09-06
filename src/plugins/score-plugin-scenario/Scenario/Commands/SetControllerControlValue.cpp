#include "SetControllerControlValue.hpp"

#include <score/document/DocumentContext.hpp>

namespace Scenario
{
// Setting the value of a controller control changes the ports of its process
// synchronously, from inside setValue() (the object's on_controller_interaction
// hook asks the model for a new port count). Like EditScript, this command
// saves the cables and the ports' data beforehand so that undo brings the
// process back to a consistent state:
//
//   redo: set the value. The ports that survive are the same objects, with
//         their cables, values and addresses; the ports that go away take
//         their cables with them (ProcessModel::removeCablesOfPorts). They
//         are not matched by index like EditScript does: a group that grows
//         moves every port after it, and the saved ports would land on the
//         wrong ones.
//   undo: remove the cables, set the old value (ports come back), reload every
//         port -- values included, the ports that were re-created start from
//         their default -- and put the cables back.
//
// In undo, inletsChanged() / outletsChanged() are emitted after the reload:
// the exec side rebuilt its ports from inside setValue(), before the
// addresses and cables were restored, and needs to see them.

SetControllerControlValue::SetControllerControlValue(
    const Process::ControlInlet& obj, ossia::value newval,
    const score::DocumentContext& ctx)
    : m_path{obj}
    , m_old{obj.value()}
    , m_new{std::move(newval)}
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
  auto& inlet = m_path.find(ctx);
  auto& proc = *safe_cast<Process::ProcessModel*>(inlet.parent());

  // Remove all the cables that could have been added during
  // the creation
  Dataflow::removeCables(m_oldCables, ctx);

  inlet.setValue(m_old);

  // We expect the inputs / outputs to revert back to the
  // exact same state
  SCORE_ASSERT(m_oldInlets.size() == proc.inlets().size());
  SCORE_ASSERT(m_oldOutlets.size() == proc.outlets().size());

  // So we can reload their data identically. The ports that were removed by
  // redo() have just been re-created with their default value: this gives
  // them their previous value back.
  for(std::size_t i = 0; i < m_oldInlets.size(); i++)
  {
    proc.inlets()[i]->loadData(m_oldInlets[i].data, Process::PortLoadDataFlags::ReloadValue);
  }
  for(std::size_t i = 0; i < m_oldOutlets.size(); i++)
  {
    proc.outlets()[i]->loadData(
        m_oldOutlets[i].data, Process::PortLoadDataFlags::ReloadValue);
  }

  // Recreate the old cables. This process's ports know them again from their
  // reloaded data; the ports at the other end do not.
  auto cables = Dataflow::restoreCablesWithoutTouchingPorts(m_oldCables, ctx);
  Dataflow::reattachCablesToPorts(cables, ctx);

  proc.inletsChanged();
  proc.outletsChanged();

  Dataflow::notifyAddedCables(cables, ctx);
}

void SetControllerControlValue::redo(const score::DocumentContext& ctx) const
{
  auto obj = m_path.try_find(ctx);
  if(!obj)
  {
    qDebug() << "Could not find: " << m_path.unsafePath().toString();
    return;
  }

  obj->setValue(m_new);
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

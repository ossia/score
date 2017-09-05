#include "AddProcessToConstraint.hpp"

namespace Scenario
{
namespace Command
{
AddProcessToConstraint::AddProcessToConstraint(
    const Scenario::ConstraintModel& constraint,
    const UuidKey<Process::ProcessModel>& process)
  : m_addProcessCommand{constraint,
                        getStrongId(constraint.processes),
                        process}
  , m_addedSlot{constraint.smallView().empty()}
{
}

void AddProcessToConstraint::undo(const iscore::DocumentContext& ctx) const
{
  auto& constraint = m_addProcessCommand.constraintPath().find(ctx);

  if(m_addedSlot)
    constraint.removeSlot(0);
  else
    constraint.removeLayer(0, m_addProcessCommand.processId());

  m_addProcessCommand.undo(ctx);
}

void AddProcessToConstraint::redo(const iscore::DocumentContext& ctx) const
{
  auto& constraint = m_addProcessCommand.constraintPath().find(ctx);

  // Create process model
  auto& proc = m_addProcessCommand.redo(constraint, ctx);

  // Make it visible
  if(m_addedSlot)
  {
    auto h
        = iscore::AppContext().settings<Scenario::Settings::Model>().getSlotHeight();
    constraint.addSlot(Slot{{proc.id()}, proc.id(), h});
    constraint.setSmallViewVisible(true);
  }
  else
  {
    constraint.addLayer(0, proc.id());
  }
}

const Path<Scenario::ConstraintModel>&AddProcessToConstraint::constraintPath() const
{
  return m_addProcessCommand.constraintPath();
}

const Id<Process::ProcessModel>&AddProcessToConstraint::processId() const
{
  return m_addProcessCommand.processId();
}

const UuidKey<Process::ProcessModel>&AddProcessToConstraint::processKey() const
{
  return m_addProcessCommand.processKey();
}

void AddProcessToConstraint::serializeImpl(DataStreamInput& s) const
{
  s << m_addProcessCommand.serialize() << m_addedSlot;
}

void AddProcessToConstraint::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> b >> m_addedSlot;

  m_addProcessCommand.deserialize(b);
}

AddProcessToConstraint::~AddProcessToConstraint()
{

}

AddProcessInNewBoxMacro::~AddProcessInNewBoxMacro()
{

}

}

}

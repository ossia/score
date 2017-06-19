#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>

#include <algorithm>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "AddOnlyProcessToConstraint.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
AddOnlyProcessToConstraint::AddOnlyProcessToConstraint(
    const ConstraintModel& cst,
    UuidKey<Process::ProcessModel>
        process)
    : AddOnlyProcessToConstraint{cst,
                                 getStrongId(cst.processes),
                                 process}
{
}

AddOnlyProcessToConstraint::AddOnlyProcessToConstraint(
    const ConstraintModel& cst,
    Id<Process::ProcessModel>
        processId,
    UuidKey<Process::ProcessModel>
        process)
    : m_path{cst}
    , m_processName{process}
    , m_createdProcessId{std::move(processId)}
{
}

void AddOnlyProcessToConstraint::undo(const iscore::DocumentContext& ctx) const
{
  undo(m_path.find(ctx));
}

void AddOnlyProcessToConstraint::redo(const iscore::DocumentContext& ctx) const
{
  redo(m_path.find(ctx));
}

void AddOnlyProcessToConstraint::undo(ConstraintModel& constraint) const
{
  RemoveProcess(constraint, m_createdProcessId);
}

Process::ProcessModel&
AddOnlyProcessToConstraint::redo(ConstraintModel& constraint) const
{
  // Create process model
  auto fac = context.interfaces<Process::ProcessFactoryList>().get(
      m_processName);
  ISCORE_ASSERT(fac);
  auto proc = fac->make(
      constraint.duration.defaultDuration(), // TODO should maybe be max ?
      m_createdProcessId,
      &constraint);

  AddProcess(constraint, proc);
  return *proc;
}

void AddOnlyProcessToConstraint::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processName << m_createdProcessId;
}

void AddOnlyProcessToConstraint::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processName >> m_createdProcessId;
}
}
}

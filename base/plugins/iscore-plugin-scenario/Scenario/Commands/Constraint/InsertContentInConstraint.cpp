
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <QDataStream>
#include <QtGlobal>
#include <algorithm>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <functional>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "InsertContentInConstraint.hpp"
#include <Process/ExpandMode.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/application/ApplicationContext.hpp>

namespace Scenario
{
namespace Command
{

InsertContentInConstraint::InsertContentInConstraint(
    QJsonObject&& sourceConstraint,
    Path<ConstraintModel>&& targetConstraint,
    ExpandMode mode)
    : m_source{std::move(sourceConstraint)}
    , m_target{std::move(targetConstraint)}
    , m_mode{mode}
{
  auto& trg_constraint = m_target.find();

  // Generate new ids for each cloned process.
  const auto& target_processes = trg_constraint.processes;
  std::vector<Id<Process::ProcessModel>> target_processes_ids;
  target_processes_ids.reserve(target_processes.size());
  std::transform(
      target_processes.begin(), target_processes.end(),
      std::back_inserter(target_processes_ids),
      [](const auto& proc) { return proc.id(); });

  for (const auto& proc : m_source["Processes"].toArray())
  {
    auto newId = getStrongId(target_processes_ids);
    m_processIds.insert(
        Id<Process::ProcessModel>(proc.toObject()["id"].toInt()), newId);
    target_processes_ids.push_back(newId);
  }
}

void InsertContentInConstraint::undo() const
{
  auto& trg_constraint = m_target.find();
  // We just have to remove what we added
  // TODO Remove the added slots, etc.

  // Remove the processes
  for (const auto& proc_id : m_processIds)
  {
    RemoveProcess(trg_constraint, proc_id);
  }
}

void InsertContentInConstraint::redo() const
{
  auto& trg_constraint = m_target.find();
  ConstraintModel src_constraint{JSONObject::Deserializer{m_source},
                                 &trg_constraint}; // Temporary parent

  std::map<const Process::ProcessModel*, Process::ProcessModel*> processPairs;

  // Clone the processes
  const auto& src_procs = src_constraint.processes;
  for (const auto& sourceproc : src_procs)
  {
    auto newproc
        = sourceproc.clone(m_processIds[sourceproc.id()], &trg_constraint);

    processPairs.insert(std::make_pair(&sourceproc, newproc));
    AddProcess(trg_constraint, newproc);

    // Resize the processes according to the new constraint.
    if (m_mode == ExpandMode::Scale)
    {
      newproc->setParentDuration(
          ExpandMode::Scale, trg_constraint.duration.defaultDuration());
    }
    else if (m_mode == ExpandMode::GrowShrink)
    {
      newproc->setParentDuration(
          ExpandMode::ForceGrow, trg_constraint.duration.defaultDuration());
    }
  }

  // TODO add the new processes to the small view
}

void InsertContentInConstraint::serializeImpl(DataStreamInput& s) const
{
  s << m_source << m_target << m_processIds << (int)m_mode;
}

void InsertContentInConstraint::deserializeImpl(DataStreamOutput& s)
{
  int mode;
  s >> m_source >> m_target >> m_processIds >> mode;
  m_mode = static_cast<ExpandMode>(mode);
}
}
}

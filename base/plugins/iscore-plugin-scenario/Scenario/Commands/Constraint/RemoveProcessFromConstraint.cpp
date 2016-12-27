#include <Process/Process.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <iscore/model/path/RelativePath.hpp>

#include <QDataStream>
#include <QtGlobal>
#include <algorithm>
#include <vector>

#include "RemoveProcessFromConstraint.hpp"
#include <Process/ProcessList.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/path/ObjectPath.hpp>

namespace Scenario
{
namespace Command
{

RemoveProcessFromConstraint::RemoveProcessFromConstraint(
    Path<ConstraintModel>&& constraintPath,
    Id<Process::ProcessModel>
        processId)
    : m_path{std::move(constraintPath)}, m_processId{std::move(processId)}
{
  auto& constraint = m_path.find();

  // Save the process
  DataStream::Serializer s1{&m_serializedProcessData};
  auto& proc = constraint.processes.at(m_processId);
  s1.readFrom(proc);

  // Save ALL the view models!
  for (const auto& lm : proc.layers())
  {
    QByteArray vm_arr;
    DataStream::Serializer s{&vm_arr};
    s.readFrom(iscore::RelativePath(*lm->parent(), lm->processModel()));
    s.readFrom(*lm);

    m_serializedViewModels.append({*lm, vm_arr});
  }
}

void RemoveProcessFromConstraint::undo() const
{
  auto& constraint = m_path.find();
  DataStream::Deserializer s{m_serializedProcessData};
  auto& fact = context.interfaces<Process::ProcessFactoryList>();
  auto proc = deserialize_interface(fact, s, &constraint);
  if (proc)
  {
    AddProcess(constraint, proc);
  }
  else
  {
    ISCORE_TODO;
    return;
  }
  // Restore the view models
  auto& layers = context.interfaces<Process::LayerFactoryList>();
  for (const auto& it : m_serializedViewModels)
  {
    const auto& path = it.first.unsafePath().vec();

    auto& slot
        = constraint.racks.at(Id<RackModel>(path.at(path.size() - 3).id()))
              .slotmodels.at(Id<SlotModel>(path.at(path.size() - 2).id()));

    DataStream::Deserializer stream{it.second};
    iscore::RelativePath process;
    stream.writeTo(process);
    auto lm = deserialize_interface(layers, stream, process, &slot);
    if (lm)
      slot.layers.add(lm);
    else
      ISCORE_TODO;
  }
}

void RemoveProcessFromConstraint::redo() const
{
  auto& constraint = m_path.find();
  RemoveProcess(constraint, m_processId);

  // The view models will be deleted accordingly.
}

void RemoveProcessFromConstraint::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_processId << m_serializedProcessData
    << m_serializedViewModels;
}

void RemoveProcessFromConstraint::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processId >> m_serializedProcessData
      >> m_serializedViewModels;
}
}
}

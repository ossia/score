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

  for(const SlotModel& slt : constraint.smallViewRack().slotmodels)
  {
    if(slt.frontLayerModel() == m_processId)
    {
      m_slots.push_back(slt);
      m_inFront.push_back(true);
    }
    else
    {
      if(ossia::find(slt.layers(), m_processId) != slt.layers().end())
      {
        m_slots.push_back(slt);
        m_inFront.push_back(false);
      }
    }
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

  for(int i = 0; i < m_slots.size(); i++)
  {
    Scenario::SlotModel& slot = m_slots[i].find();
    slot.addLayer(m_processId);
    if(m_inFront[i])
      slot.putToFront(m_processId);
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
    << m_slots << m_inFront;
}

void RemoveProcessFromConstraint::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_processId >> m_serializedProcessData
    >> m_slots >> m_inFront;
}
}
}

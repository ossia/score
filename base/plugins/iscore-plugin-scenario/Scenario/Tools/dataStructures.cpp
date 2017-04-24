#include <Process/ProcessList.hpp>
#include <QDataStream>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/application/ApplicationContext.hpp>

#include "dataStructures.hpp"
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
ConstraintSaveData::ConstraintSaveData(
    const Scenario::ConstraintModel& constraint)
    : constraintPath{constraint}
{
  processes.reserve(constraint.processes.size());
  for (const auto& process : constraint.processes)
  {
    QByteArray arr;
    DataStream::Serializer s{&arr};
    s.readFrom(process);
    processes.push_back(std::move(arr));
  }

  racks.reserve(2);

  {
    QByteArray arr;
    DataStream::Serializer s{&arr};
    // We only save the data proper to the racks.
    s.read(constraint.smallViewRack());
    racks.push_back(std::move(arr));
  }

  {
    QByteArray arr;
    DataStream::Serializer s{&arr};
    // We only save the data proper to the racks.
    s.read(constraint.fullViewRack());
    racks.push_back(std::move(arr));
  }
}

void ConstraintSaveData::reload(Scenario::ConstraintModel& constraint) const
{
  auto& comps = iscore::AppComponents();
  auto& procsfactories = comps.interfaces<Process::ProcessFactoryList>();
  for (auto& sourceproc : processes)
  {
    DataStream::Deserializer des{sourceproc};
    auto proc = deserialize_interface(procsfactories, des, &constraint);
    if (proc)
      AddProcess(constraint, proc);
    else
      ISCORE_TODO;
  }

  // Restore the rackes
  {
    DataStream::Deserializer des{racks[0]};
    des.write(*constraint.m_smallViewRack);
  }
  {
    DataStream::Deserializer des{racks[1]};
    des.write(*constraint.m_fullViewRack);
  }
}
}
template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::TimenodeProperties& timenodeProperties)
{
  m_stream << timenodeProperties.oldDate << timenodeProperties.newDate;

  insertDelimiter();
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::TimenodeProperties& timenodeProperties)
{

  m_stream >> timenodeProperties.oldDate >> timenodeProperties.newDate;

  checkDelimiter();
}

//----------

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::ConstraintSaveData& constraintProperties)
{
  m_stream << constraintProperties.constraintPath
           << constraintProperties.processes << constraintProperties.racks;
  insertDelimiter();
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::ConstraintSaveData& constraintProperties)
{
  m_stream >> constraintProperties.constraintPath
      >> constraintProperties.processes >> constraintProperties.racks;

  checkDelimiter();
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::ConstraintProperties& constraintProperties)
{
  m_stream << constraintProperties.oldMin << constraintProperties.newMin
           << constraintProperties.oldMax << constraintProperties.newMax;

  readFrom(
      static_cast<const Scenario::ConstraintSaveData&>(constraintProperties));

  insertDelimiter();
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::ConstraintProperties& constraintProperties)
{
  m_stream >> constraintProperties.oldMin >> constraintProperties.newMin
      >> constraintProperties.oldMax >> constraintProperties.newMax;

  writeTo(static_cast<Scenario::ConstraintSaveData&>(constraintProperties));
  checkDelimiter();
}

//----------
template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::ElementsProperties& elementsProperties)
{
  m_stream << elementsProperties.timenodes << elementsProperties.constraints;

  insertDelimiter();
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::ElementsProperties& elementsProperties)
{

  m_stream >> elementsProperties.timenodes >> elementsProperties.constraints;

  checkDelimiter();
}

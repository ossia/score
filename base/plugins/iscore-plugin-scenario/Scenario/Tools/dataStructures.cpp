// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ProcessList.hpp>
#include <QDataStream>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
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
    s.readFrom(constraint.smallView());
    racks.push_back(std::move(arr));
  }

  {
    QByteArray arr;
    DataStream::Serializer s{&arr};
    // We only save the data proper to the racks.
    s.readFrom(constraint.fullView());
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
    Scenario::Rack r;
    des.writeTo(r);
    constraint.replaceSmallView(std::move(r));
  }
  {
    DataStream::Deserializer des{racks[1]};
    Scenario::FullRack r;
    des.writeTo(r);
    constraint.replaceFullView(std::move(r));
  }
}
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::TimenodeProperties& timesyncProperties)
{
  m_stream << timesyncProperties.oldDate << timesyncProperties.newDate;

  insertDelimiter();
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::TimenodeProperties& timesyncProperties)
{

  m_stream >> timesyncProperties.oldDate >> timesyncProperties.newDate;

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
  m_stream << constraintProperties.oldDefault
           << constraintProperties.oldMin << constraintProperties.newMin
           << constraintProperties.oldMax << constraintProperties.newMax;

  readFrom(
      static_cast<const Scenario::ConstraintSaveData&>(constraintProperties));

  insertDelimiter();
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::ConstraintProperties& constraintProperties)
{
  m_stream
      >> constraintProperties.oldDefault
      >> constraintProperties.oldMin >> constraintProperties.newMin
      >> constraintProperties.oldMax >> constraintProperties.newMax;

  writeTo(static_cast<Scenario::ConstraintSaveData&>(constraintProperties));
  checkDelimiter();
}

//----------
template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::ElementsProperties& elementsProperties)
{
  m_stream << elementsProperties.timesyncs << elementsProperties.constraints;

  insertDelimiter();
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::ElementsProperties& elementsProperties)
{

  m_stream >> elementsProperties.timesyncs >> elementsProperties.constraints;

  checkDelimiter();
}

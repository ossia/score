// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "dataStructures.hpp"

#include <Process/ProcessList.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/MapSerialization.hpp>

#include <score_plugin_scenario_export.h>

namespace Scenario
{
IntervalSaveData::IntervalSaveData(const Scenario::IntervalModel& interval, bool saveIntemporal)
    : intervalPath{interval}
{
  processes.reserve(interval.processes.size());
  if (saveIntemporal)
  {
    for (const auto& process : interval.processes)
    {
      QByteArray arr;
      DataStream::Serializer s{&arr};
      s.readFrom(process);
      processes.push_back(std::move(arr));
    }
  }
  else
  {
    for (const auto& process : interval.processes)
    {
      if (!(process.flags() & Process::ProcessFlags::TimeIndependent))
      {
        QByteArray arr;
        DataStream::Serializer s{&arr};
        s.readFrom(process);
        processes.push_back(std::move(arr));
      }
    }
  }
  racks.reserve(2);

  {
    QByteArray arr;
    DataStream::Serializer s{&arr};
    // We only save the data proper to the racks.
    s.readFrom(interval.smallView());
    racks.push_back(std::move(arr));
  }

  {
    QByteArray arr;
    DataStream::Serializer s{&arr};
    // We only save the data proper to the racks.
    s.readFrom(interval.fullView());
    racks.push_back(std::move(arr));
  }
}

void IntervalSaveData::reload(Scenario::IntervalModel& interval) const
{
  auto& comps = score::AppComponents();
  auto& procsfactories = comps.interfaces<Process::ProcessFactoryList>();
  for (auto& sourceproc : processes)
  {
    DataStream::Deserializer des{sourceproc};
    auto proc = deserialize_interface(procsfactories, des, interval.context(), &interval);
    if (proc)
      AddProcess(interval, proc);
    else
      SCORE_TODO;
  }

  // Restore the rackes
  {
    DataStream::Deserializer des{racks[0]};
    Scenario::Rack r;
    des.writeTo(r);
    interval.replaceSmallView(std::move(r));
  }
  {
    DataStream::Deserializer des{racks[1]};
    Scenario::FullRack r;
    des.writeTo(r);
    interval.replaceFullView(std::move(r));
  }
}
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::TimenodeProperties& timesyncProperties)
{
  m_stream << timesyncProperties.oldDate << timesyncProperties.newDate;

  insertDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::TimenodeProperties& timesyncProperties)
{

  m_stream >> timesyncProperties.oldDate >> timesyncProperties.newDate;

  checkDelimiter();
}

//----------

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::IntervalSaveData& intervalProperties)
{
  m_stream << intervalProperties.intervalPath << intervalProperties.processes
           << intervalProperties.racks;
  insertDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::IntervalSaveData& intervalProperties)
{
  m_stream >> intervalProperties.intervalPath >> intervalProperties.processes
      >> intervalProperties.racks;

  checkDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::IntervalProperties& intervalProperties)
{
  m_stream << intervalProperties.oldDefault << intervalProperties.oldMin
           << intervalProperties.newMin << intervalProperties.oldMax << intervalProperties.newMax;

  readFrom(static_cast<const Scenario::IntervalSaveData&>(intervalProperties));

  insertDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::IntervalProperties& intervalProperties)
{
  m_stream >> intervalProperties.oldDefault >> intervalProperties.oldMin
      >> intervalProperties.newMin >> intervalProperties.oldMax >> intervalProperties.newMax;

  writeTo(static_cast<Scenario::IntervalSaveData&>(intervalProperties));
  checkDelimiter();
}

//----------
template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::ElementsProperties& elementsProperties)
{
  m_stream << elementsProperties.timesyncs << elementsProperties.intervals
           << elementsProperties.cables;

  insertDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::ElementsProperties& elementsProperties)
{
  m_stream >> elementsProperties.timesyncs >> elementsProperties.intervals
      >> elementsProperties.cables;

  checkDelimiter();
}

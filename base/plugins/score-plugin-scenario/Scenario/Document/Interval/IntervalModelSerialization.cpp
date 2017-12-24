// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/tools/std/Optional.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <sys/types.h>

#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/PathSerialization.hpp>


namespace Scenario
{
class StateModel;
}
template <typename T>
class IdentifiedObject;
template <typename T>
class Reader;
template <typename T>
class Writer;

// Note : comment gérer le cas d'un process shared model qui ne sait se
// sérializer qu'en binaire, dans du json?
// Faire passer l'info en base64 ?


template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::Slot& slot)
{
  m_stream << slot.processes << slot.frontProcess << slot.height;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::Slot& slot)
{
  m_stream >> slot.processes >> slot.frontProcess >> slot.height;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONObjectReader::read(
    const Scenario::Slot& slot)
{
  obj[strings.Processes] = toJsonValueArray(slot.processes);
  obj[strings.Process] = toJsonValue(slot.frontProcess);
  obj[strings.Height] = slot.height;
}
template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONObjectWriter::write(
    Scenario::Slot& slot)
{
  slot.processes = fromJsonValueArray<decltype(slot.processes)>(obj[strings.Processes].toArray());
  slot.frontProcess = fromJsonValue<decltype(slot.frontProcess)>(obj[strings.Process]);
  slot.height = obj[strings.Height].toDouble();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::FullSlot& slot)
{
  m_stream << slot.process;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::FullSlot& slot)
{
  m_stream >> slot.process;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONObjectReader::read(
    const Scenario::FullSlot& slot)
{
  obj[strings.Process] = toJsonValue(slot.process);
}
template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONObjectWriter::write(
    Scenario::FullSlot& slot)
{
  slot.process = fromJsonValue<decltype(slot.process)>(obj[strings.Process]);
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::SlotPath& slot)
{
  m_stream << slot.interval << slot.index << slot.full_view;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::SlotPath& slot)
{
  m_stream >> slot.interval >> slot.index >> slot.full_view;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::IntervalModel& interval)
{
  insertDelimiter();
  // Ports
  m_stream << *interval.inlet << *interval.outlet;

  // Processes
  m_stream << (int32_t)interval.processes.size();
  for (const auto& process : interval.processes)
  {
    readFrom(process);
  }

  // Racks
  m_stream << interval.m_smallView << interval.m_fullView;

  // Common data
  m_stream << interval.duration << interval.m_startState
           << interval.m_endState

           << interval.m_date << interval.m_heightPercentage
           << interval.m_zoom << interval.m_center
           << interval.m_smallViewShown;


  insertDelimiter();
}


template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::IntervalModel& interval)
{
  checkDelimiter();
  // Ports
  interval.inlet = Process::make_inlet(*this, &interval);
  interval.outlet = Process::make_outlet(*this, &interval);

  // Processes
  int32_t process_count;
  m_stream >> process_count;

  static auto& pl = components.interfaces<Process::ProcessFactoryList>();
  for (; process_count-- > 0;)
  {
    auto proc = deserialize_interface(pl, *this, &interval);
    if (proc)
    {
      // TODO why isn't AddProcess used here ?!
      interval.processes.add(proc);
    }
    else
    {
      SCORE_TODO;
    }
  }

  // Rackes
  m_stream >> interval.m_smallView >> interval.m_fullView;

  // Common data
  m_stream >> interval.duration >> interval.m_startState
      >> interval.m_endState

      >> interval.m_date >> interval.m_heightPercentage
      >> interval.m_zoom >> interval.m_center
      >> interval.m_smallViewShown;

  checkDelimiter();
}


template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONObjectReader::read(
    const Scenario::IntervalModel& interval)
{
  // Ports
  obj["Inlet"] = toJsonObject(*interval.inlet);
  obj["Outlet"] = toJsonObject(*interval.outlet);

  // Processes
  obj[strings.Processes] = toJsonArray(interval.processes);

  // Rackes
  obj[strings.SmallViewRack] = toJsonArray(interval.smallView());
  obj[strings.FullViewRack] = toJsonArray(interval.fullView());

  // Common data
  // The fields will go in the same level as the
  // rest of the interval
  readFrom(interval.duration);

  obj[strings.StartState] = toJsonValue(interval.m_startState);
  obj[strings.EndState] = toJsonValue(interval.m_endState);

  obj[strings.StartDate] = toJsonValue(interval.m_date);
  obj[strings.HeightPercentage] = interval.m_heightPercentage;

  obj[strings.Zoom] = interval.m_zoom;
  obj[strings.Center] = toJsonValue(interval.m_center);
  obj[strings.SmallViewShown] = interval.m_smallViewShown;
}


template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectWriter::write(Scenario::IntervalModel& interval)
{
  {
    JSONObjectWriter writer{obj["Inlet"].toObject()};
    interval.inlet = Process::make_inlet(writer, &interval);
  }
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    interval.outlet = Process::make_outlet(writer, &interval);
  }

  static auto& pl = components.interfaces<Process::ProcessFactoryList>();

  QJsonArray process_array = obj[strings.Processes].toArray();
  for (const auto& json_vref : process_array)
  {
    JSONObject::Deserializer deserializer{json_vref.toObject()};
    auto proc = deserialize_interface(pl, deserializer, &interval);
    if (proc)
      interval.processes.add(proc);
    else
      SCORE_TODO;
  }

  auto sv_it = obj.constFind(strings.SmallViewRack);
  if(sv_it != obj.constEnd())
  {
    fromJsonArray(sv_it->toArray(), interval.m_smallView);
    interval.m_smallViewShown = obj[strings.SmallViewShown].toBool();
  }
  else if(!interval.processes.empty())
  {
    // To support old scores...
    Scenario::Slot s;
    for(auto& proc : interval.processes)
      s.processes.push_back(proc.id());
    s.frontProcess = s.processes.front();
    interval.m_smallView.push_back(s);
    interval.m_smallViewShown = true;
  }

  auto fv_it = obj.constFind(strings.FullViewRack);
  if(fv_it != obj.constEnd())
  {
    fromJsonArray(fv_it->toArray(), interval.m_fullView);
  }
  else
  {
    // Create a slot for every process
    interval.m_fullView.clear();
    for(auto& proc : interval.processes)
      interval.m_fullView.push_back(Scenario::FullSlot{proc.id()});
  }

  writeTo(interval.duration);
  interval.m_startState
      = fromJsonValue<Id<Scenario::StateModel>>(obj[strings.StartState]);
  interval.m_endState
      = fromJsonValue<Id<Scenario::StateModel>>(obj[strings.EndState]);

  interval.m_date = fromJsonValue<TimeVal>(obj[strings.StartDate]);
  interval.m_heightPercentage = obj[strings.HeightPercentage].toDouble();

  auto zit = obj.find(strings.Zoom);
  if(zit != obj.end())
    interval.m_zoom = zit->toDouble();
  auto cit = obj.find(strings.Center);
  if(cit != obj.end() && cit->isDouble())
    interval.m_center = fromJsonValue<TimeVal>(*cit);


}


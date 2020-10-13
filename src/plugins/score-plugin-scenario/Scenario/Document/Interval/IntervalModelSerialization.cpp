// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/EntityMapSerialization.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <score/tools/std/Optional.hpp>

static_assert(is_template<Scenario::Rack>::value);
static_assert(is_template<Scenario::FullRack>::value);
template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(const ossia::time_signature& slot)
{
  m_stream << slot.upper << slot.lower;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(ossia::time_signature& slot)
{
  m_stream >> slot.upper >> slot.lower;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONReader::read(const ossia::time_signature& slot)
{
  stream.StartArray();
  stream.Int(slot.upper);
  stream.Int(slot.lower);
  stream.EndArray();
}
template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONWriter::write(ossia::time_signature& slot)
{
  const auto& arr = base.GetArray();
  slot.upper = arr[0].GetInt();
  slot.lower = arr[1].GetInt();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(const Scenario::Slot& slot)
{
  m_stream << slot.processes << slot.frontProcess << slot.height << slot.nodal;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(Scenario::Slot& slot)
{
  m_stream >> slot.processes >> slot.frontProcess >> slot.height >> slot.nodal;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONReader::read(const Scenario::Slot& slot)
{
  stream.StartObject();
  obj[strings.Processes] = slot.processes;
  obj[strings.Process] = slot.frontProcess;
  obj[strings.Height] = slot.height;
  obj["Nodal"] = slot.nodal;
  stream.EndObject();
}
template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONWriter::write(Scenario::Slot& slot)
{
  slot.processes <<= obj[strings.Processes];
  slot.frontProcess <<= obj[strings.Process];
  slot.height = obj[strings.Height].toDouble();
  slot.nodal = obj["Nodal"].toBool();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(const Scenario::FullSlot& slot)
{
  m_stream << slot.process << slot.nodal;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(Scenario::FullSlot& slot)
{
  m_stream >> slot.process >> slot.nodal;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONReader::read(const Scenario::FullSlot& slot)
{
  stream.StartObject();
  obj[strings.Process] = slot.process;
  obj["Nodal"] = slot.nodal;
  stream.EndObject();
}
template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONWriter::write(Scenario::FullSlot& slot)
{
  slot.process <<= obj[strings.Process];
  slot.nodal = obj["Nodal"].toBool();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(const Scenario::SlotPath& slot)
{
  m_stream << slot.interval << slot.index << slot.full_view;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(Scenario::SlotPath& slot)
{
  m_stream >> slot.interval >> slot.index >> slot.full_view;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(const Scenario::IntervalModel& interval)
{
  insertDelimiter();
  m_stream << interval.m_graphal;
  if (interval.m_graphal)
  {
    m_stream << interval.duration << interval.m_startState << interval.m_endState
             << interval.m_date << interval.m_heightPercentage;
    return;
  }

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

  m_stream << interval.m_signatures << interval.duration << interval.m_startState
           << interval.m_endState

           << interval.m_date << interval.m_heightPercentage << interval.m_nodalFullViewSlotHeight << interval.m_zoom
           << interval.m_center << interval.m_viewMode << interval.m_smallViewShown
           << interval.m_hasSignature;

  insertDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(Scenario::IntervalModel& interval)
{
  checkDelimiter();
  bool gr{};
  m_stream >> gr;
  if ((interval.m_graphal = gr))
  {
    m_stream >> interval.duration >> interval.m_startState >> interval.m_endState
        >> interval.m_date >> interval.m_heightPercentage;
    return;
  }

  // Ports
  interval.inlet = Process::load_audio_inlet(*this, &interval);
  interval.outlet = Process::load_audio_outlet(*this, &interval);

  // Processes
  int32_t process_count;
  m_stream >> process_count;

  static auto& pl = components.interfaces<Process::ProcessFactoryList>();
  for (; process_count-- > 0;)
  {
    auto proc = deserialize_interface(pl, *this, interval.context(), &interval);
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
  Scenario::IntervalModel::ViewMode vm{Scenario::IntervalModel::ViewMode::Temporal};
  bool sv{};
  bool hs{};
  m_stream >> interval.m_signatures >> interval.duration >> interval.m_startState
      >> interval.m_endState

      >> interval.m_date >> interval.m_heightPercentage >> interval.m_nodalFullViewSlotHeight >> interval.m_zoom >> interval.m_center
      >> vm >> sv >> hs;
  interval.m_viewMode = vm;
  interval.m_smallViewShown = sv;
  interval.m_hasSignature = hs;

  checkDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONReader::read(const Scenario::IntervalModel& interval)
{
  if (interval.graphal())
  {
    obj["Graphal"] = true;
    readFrom(interval.duration);
    obj[strings.StartState] = interval.m_startState;
    obj[strings.EndState] = interval.m_endState;

    obj[strings.StartDate] = interval.m_date;
    obj[strings.HeightPercentage] = interval.m_heightPercentage;
    return;
  }

  // Ports
  obj["Inlet"] = *interval.inlet;
  obj["Outlet"] = *interval.outlet;

  // Processes
  obj[strings.Processes] = interval.processes;

  // Rackes
  obj[strings.SmallViewRack] = interval.smallView();
  obj[strings.FullViewRack] = interval.fullView();

  // Common data

  // The fields will go in the same level as the
  // rest of the interval
  readFrom(interval.duration);

  obj["Signatures"] = interval.m_signatures;

  obj[strings.StartState] = interval.m_startState;
  obj[strings.EndState] = interval.m_endState;

  obj[strings.StartDate] = interval.m_date;
  obj[strings.HeightPercentage] = interval.m_heightPercentage;
  obj["NodalSlotHeight"] = interval.m_nodalFullViewSlotHeight;

  obj[strings.Zoom] = interval.m_zoom;
  obj[strings.Center] = interval.m_center;
  obj["ViewMode"] = (int)interval.m_viewMode;
  obj[strings.SmallViewShown] = interval.m_smallViewShown;

  obj["HasSignature"] = interval.m_hasSignature;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONWriter::write(Scenario::IntervalModel& interval)
{
  if (auto it = obj.tryGet("Graphal"); it && it->toBool())
  {
    interval.m_graphal = true;
    writeTo(interval.duration);

    interval.m_startState <<= obj[strings.StartState];
    interval.m_endState <<= obj[strings.EndState];

    interval.m_date <<= obj[strings.StartDate];
    interval.m_heightPercentage = obj[strings.HeightPercentage].toDouble();
    return;
  }

  {
    JSONWriter writer{obj["Inlet"]};
    interval.inlet = Process::load_audio_inlet(writer, &interval);
  }
  {
    JSONWriter writer{obj["Outlet"]};
    interval.outlet = Process::load_audio_outlet(writer, &interval);
  }

  static auto& pl = components.interfaces<Process::ProcessFactoryList>();

  const auto& process_array = obj[strings.Processes].toArray();
  for (const auto& json_vref : process_array)
  {
    JSONObject::Deserializer deserializer{json_vref};
    auto proc = deserialize_interface(pl, deserializer, interval.context(), &interval);
    if (proc)
      interval.processes.add(proc);
    else
      SCORE_TODO;
  }

  auto sv_it = obj.constFind(strings.SmallViewRack);
  if (sv_it != obj.constEnd())
  {
    interval.m_smallView <<= *sv_it;
    interval.m_smallViewShown = obj[strings.SmallViewShown].toBool();
  }
  else if (!interval.processes.empty())
  {
    // To support old scores...
    Scenario::Slot s;
    for (auto& proc : interval.processes)
      s.processes.push_back(proc.id());
    s.frontProcess = s.processes.front();
    interval.m_smallView.push_back(s);
    interval.m_smallViewShown = true;
  }

  auto fv_it = obj.constFind(strings.FullViewRack);
  if (fv_it != obj.constEnd())
  {
    interval.m_fullView <<= *fv_it;
  }
  else
  {
    // Create a slot for every process
    interval.m_fullView.clear();
    for (auto& proc : interval.processes)
      interval.m_fullView.push_back(Scenario::FullSlot{proc.id()});
  }

  writeTo(interval.duration);

  interval.m_signatures <<= obj["Signatures"];

  interval.m_startState <<= obj[strings.StartState];
  interval.m_endState <<= obj[strings.EndState];

  interval.m_date <<= obj[strings.StartDate];
  interval.m_heightPercentage = obj[strings.HeightPercentage].toDouble();
  interval.m_nodalFullViewSlotHeight = obj["NodalSlotHeight"].toDouble();
  interval.m_viewMode = static_cast<Scenario::IntervalModel::ViewMode>(obj["ViewMode"].toInt());

  auto zit = obj.constFind(strings.Zoom);
  if (zit != obj.constEnd())
    interval.m_zoom = zit->toDouble();
  auto cit = obj.constFind(strings.Center);
  if (cit != obj.constEnd() && cit->isDouble())
    interval.m_center <<= *cit;

  interval.m_hasSignature = obj["HasSignature"].toBool();
}

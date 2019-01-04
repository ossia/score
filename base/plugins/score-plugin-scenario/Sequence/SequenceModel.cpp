// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "SequenceModel.hpp"
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <score/selection/SelectionStack.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Sequence::ProcessModel)

namespace Sequence
{

ProcessModel::ProcessModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id,
                            Metadata<
                                ObjectKey_k, Sequence::ProcessModel>::get(),
                            parent}
    , inlet{Process::make_inlet(Id<Process::Port>(0), this)}
    , outlet{Process::make_outlet(Id<Process::Port>(0), this)}
    , m_startTimeSyncId{Scenario::startId<Scenario::TimeSyncModel>()}
    , m_startEventId{Scenario::startId<Scenario::EventModel>()}
    , m_startStateId{Scenario::startId<Scenario::StateModel>()}
{
  auto start_ts = new Scenario::TimeSyncModel{m_startTimeSyncId, {0., 0.1}, TimeVal::zero(), this};
  start_ts->metadata().setName("Sync.start");
  timeSyncs.add(start_ts);

  auto start_ev = new Scenario::EventModel{m_startEventId, start_ts->id(), {0., 0.0}, TimeVal::zero(), this};
  start_ev->metadata().setName("Event.start");
  events.add(start_ev);
  start_ts->addEvent(start_ev->id());

  auto start_st = new Scenario::StateModel{m_startStateId, start_ev->id(), 0.02, this};
  start_st->metadata().setName("State.start");
  states.add(start_st);
  start_ev->addState(start_st->id());

  // At the end because plug-ins depend on the start/end timesync & al being
  // here
  metadata().setInstanceName(*this);

  inlet->type = Process::PortType::Audio;
  outlet->type = Process::PortType::Audio;
  outlet->setPropagate(true);

  init();
}

void ProcessModel::init()
{
  inlet->setCustomData("In");
  outlet->setCustomData("Out");
  m_inlets.push_back(inlet.get());
  m_outlets.push_back(outlet.get());
}

ProcessModel::~ProcessModel()
{
  try
  {
    score::IDocument::documentContext(*parent()).selectionStack.clear();
  }
  catch (...)
  {
    // Sometimes the scenario isn't in the hierarchy, e.G. in
    // ScenarioPasteElements
  }
  intervals.clear();
  states.clear();
  events.clear();
  timeSyncs.clear();

  identified_object_destroying(this);
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  double scale = newDuration / duration();

  for (auto& timesync : timeSyncs)
  {
    timesync.setDate(timesync.date() * scale);
    // Since events will also move we do not need
    // to move the timesync.
  }

  for (auto& event : events)
  {
    event.setDate(event.date() * scale);
    eventMoved(event);
  }

  for (auto& interval : intervals)
  {
    interval.setStartDate(interval.date() * scale);
    // Note : scale the min / max.

    auto newdur = interval.duration.defaultDuration() * scale;
    Scenario::IntervalDurations::Algorithms::scaleAllDurations(interval, newdur);

    for (auto& process : interval.processes)
    {
      process.setParentDuration(ExpandMode::Scale, newdur);
    }

    intervalMoved(interval);
  }

  this->setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  this->setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  this->setDuration(newDuration);
}

void ProcessModel::startExecution()
{
  // TODO this is called for each process!!
  // But it should be done only once at the global level.
  for (auto& interval : intervals)
  {
    interval.startExecution();
  }
}

void ProcessModel::stopExecution()
{
  for (auto& interval : intervals)
  {
    interval.stopExecution();
  }
  for (auto& ev : events)
  {
    ev.setStatus(Scenario::ExecutionStatus::Editing, *this);
  }
}

void ProcessModel::reset()
{
  for (auto& interval : intervals)
  {
    interval.reset();
  }

  for (auto& event : events)
  {
    event.setStatus(Scenario::ExecutionStatus::Editing, *this);
  }
}

Selection ProcessModel::selectableChildren() const noexcept
{
  Selection objects;
  apply([&](const auto& m) {
    for (auto& elt : this->*m)
      objects.append(&elt);
  });
  return objects;
}

template <typename InputVec, typename OutputVec>
static void copySelected(const InputVec& in, OutputVec& out)
{
  for (const auto& elt : in)
  {
    if (elt.selection.get())
      out.append(&elt);
  }
}

Selection ProcessModel::selectedChildren() const noexcept
{
  Selection objects;
  apply([&](const auto& m) { copySelected(this->*m, objects); });
  return objects;
}

void ProcessModel::setSelection(const Selection& s) const noexcept
{
  // OPTIMIZEME
  apply([&](auto&& m) {
    for (auto& elt : this->*m)
      elt.selection.set(s.contains(&elt));
  });
}

const QVector<Id<Scenario::IntervalModel>> intervalsBeforeTimeSync(
    const Sequence::ProcessModel& scenar, const Id<Scenario::TimeSyncModel>& timeSyncId)
{
  QVector<Id<Scenario::IntervalModel>> cstrs;
  const auto& tn = scenar.timeSyncs.at(timeSyncId);
  for (const auto& ev : tn.events())
  {
    const auto& evM = scenar.events.at(ev);
    for (const auto& st : evM.states())
    {
      const auto& stM = scenar.states.at(st);
      if (stM.previousInterval())
        cstrs.push_back(*stM.previousInterval());
    }
  }

  return cstrs;
}

bool ProcessModel::contentHasDuration() const noexcept
{
  return true;
}

TimeVal ProcessModel::contentDuration() const noexcept
{
  TimeVal max_tn_pos = TimeVal::zero();
  for (auto& t : timeSyncs)
  {
    if (t.date() > max_tn_pos)
      max_tn_pos = t.date();
  }
  return max_tn_pos;
}

}


template <>
void DataStreamReader::read(const Sequence::ProcessModel& scenario)
{
  // Ports
  m_stream << *scenario.inlet << *scenario.outlet;

  m_stream << scenario.m_startTimeSyncId;
  m_stream << scenario.m_startEventId;
  m_stream << scenario.m_startStateId;

  // Intervals
  const auto& intervals = scenario.intervals;
  m_stream << (int32_t)intervals.size();

  for (const auto& interval : intervals)
  {
    readFrom(interval);
  }

  // Timenodes
  const auto& timesyncs = scenario.timeSyncs;
  m_stream << (int32_t)timesyncs.size();

  for (const auto& timesync : timesyncs)
  {
    readFrom(timesync);
  }

  // Events
  const auto& events = scenario.events;
  m_stream << (int32_t)events.size();

  for (const auto& event : events)
  {
    readFrom(event);
  }

  // States
  const auto& states = scenario.states;
  m_stream << (int32_t)states.size();

  for (const auto& state : states)
  {
    readFrom(state);
  }

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Sequence::ProcessModel& scenario)
{
  // Ports
  scenario.inlet = Process::make_inlet(*this, &scenario);
  scenario.outlet = Process::make_outlet(*this, &scenario);

  m_stream >> scenario.m_startTimeSyncId;
  m_stream >> scenario.m_startEventId;
  m_stream >> scenario.m_startStateId;

  // Intervals
  int32_t interval_count;
  m_stream >> interval_count;

  for (; interval_count-- > 0;)
  {
    auto interval = new Scenario::IntervalModel{*this, &scenario};
    scenario.intervals.add(interval);
  }

  // Timenodes
  int32_t timesync_count;
  m_stream >> timesync_count;

  for (; timesync_count-- > 0;)
  {
    auto tnmodel = new Scenario::TimeSyncModel{*this, &scenario};
    scenario.timeSyncs.add(tnmodel);
  }

  // Events
  int32_t event_count;
  m_stream >> event_count;

  for (; event_count-- > 0;)
  {
    auto evmodel = new Scenario::EventModel{*this, &scenario};
    scenario.events.add(evmodel);
  }

  // States
  int32_t state_count;
  m_stream >> state_count;

  for (; state_count-- > 0;)
  {
    auto stmodel = new Scenario::StateModel{*this, &scenario};
    scenario.states.add(stmodel);
  }

  // Finally, we re-set the intervals before and after the states
  for (const Scenario::IntervalModel& interval : scenario.intervals)
  {
    Scenario::SetPreviousInterval(
        scenario.states.at(interval.endState()), interval);
    Scenario::SetNextInterval(
        scenario.states.at(interval.startState()), interval);
  }

  // Scenario::ScenarioValidityChecker::checkValidity(scenario);
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Sequence::ProcessModel& scenario)
{
  obj["Inlet"] = toJsonObject(*scenario.inlet);
  obj["Outlet"] = toJsonObject(*scenario.outlet);
  obj["StartTimeNodeId"] = toJsonValue(scenario.m_startTimeSyncId);
  obj["StartEventId"] = toJsonValue(scenario.m_startEventId);
  obj["StartStateId"] = toJsonValue(scenario.m_startStateId);

  obj["TimeNodes"] = toJsonArray(scenario.timeSyncs);
  obj["Events"] = toJsonArray(scenario.events);
  obj["States"] = toJsonArray(scenario.states);
  obj["Constraints"] = toJsonArray(scenario.intervals);
}

template <>
void JSONObjectWriter::write(Sequence::ProcessModel& scenario)
{
  {
    JSONObjectWriter writer{obj["Inlet"].toObject()};
    scenario.inlet = Process::make_inlet(writer, &scenario);
    if (!scenario.inlet)
    {
      scenario.inlet = Process::make_inlet(Id<Process::Port>(0), &scenario);
      scenario.inlet->type = Process::PortType::Audio;
    }
  }
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    scenario.outlet = Process::make_outlet(writer, &scenario);

    if (!scenario.outlet)
    {
      scenario.outlet = Process::make_outlet(Id<Process::Port>(0), &scenario);
      scenario.outlet->type = Process::PortType::Audio;
    }
  }

  scenario.m_startTimeSyncId
      = fromJsonValue<Id<Scenario::TimeSyncModel>>(obj["StartTimeNodeId"]);
  scenario.m_startEventId
      = fromJsonValue<Id<Scenario::EventModel>>(obj["StartEventId"]);
  scenario.m_startStateId
      = fromJsonValue<Id<Scenario::StateModel>>(obj["StartStateId"]);

  const auto& intervals = obj["Constraints"].toArray();
  for (const auto& json_vref : intervals)
  {
    auto interval = new Scenario::IntervalModel{
        JSONObject::Deserializer{json_vref.toObject()}, &scenario};
    scenario.intervals.add(interval);
  }

  const auto& timesyncs = obj["TimeNodes"].toArray();
  for (const auto& json_vref : timesyncs)
  {
    auto tnmodel = new Scenario::TimeSyncModel{
        JSONObject::Deserializer{json_vref.toObject()}, &scenario};

    scenario.timeSyncs.add(tnmodel);
  }

  const auto& events = obj["Events"].toArray();
  for (const auto& json_vref : events)
  {
    auto evmodel = new Scenario::EventModel{
        JSONObject::Deserializer{json_vref.toObject()}, &scenario};

    scenario.events.add(evmodel);
  }

  const auto& states = obj["States"].toArray();
  for (const auto& json_vref : states)
  {
    auto stmodel = new Scenario::StateModel{
        JSONObject::Deserializer{json_vref.toObject()}, &scenario};

    scenario.states.add(stmodel);
  }

  // Finally, we re-set the intervals before and after the states
  for (const Scenario::IntervalModel& interval : scenario.intervals)
  {
    Scenario::SetPreviousInterval(
        scenario.states.at(interval.endState()), interval);
    Scenario::SetNextInterval(
        scenario.states.at(interval.startState()), interval);
  }

  // Scenario::ScenarioValidityChecker::checkValidity(scenario);
}

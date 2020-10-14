// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ScenarioModel.hpp"

#include "Algorithms/StandardCreationPolicy.hpp"

#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Graph.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/ScenarioProcessMetadata.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/selection/Selectable.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>

#include <wobjectimpl.h>

#include <vector>
W_OBJECT_IMPL(Scenario::ProcessModel)
namespace Scenario
{

ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    const score::DocumentContext& ctx,
    QObject* parent)
    : Process::
        ProcessModel{duration, id, Metadata<ObjectKey_k, Scenario::ProcessModel>::get(), parent}
    , inlet{Process::make_audio_inlet(Id<Process::Port>(0), this)}
    , outlet{Process::make_audio_outlet(Id<Process::Port>(0), this)}
    , m_context{ctx}
    , m_startTimeSyncId{Scenario::startId<TimeSyncModel>()}
    , m_startEventId{Scenario::startId<EventModel>()}
    , m_startStateId{Scenario::startId<StateModel>()}
{
  auto& start_tn = ScenarioCreate<TimeSyncModel>::redo(m_startTimeSyncId, TimeVal::zero(), *this);
  start_tn.metadata().setName("Sync.start");
  start_tn.setStartPoint(true);

  auto& start_ev = ScenarioCreate<EventModel>::redo(m_startEventId, start_tn, *this);
  start_ev.metadata().setName("Event.start");
  auto& start_st = ScenarioCreate<StateModel>::redo(m_startStateId, start_ev, 0.02, *this);
  start_st.metadata().setName("State.start");
  // At the end because plug-ins depend on the start/end timesync & al being
  // here
  metadata().setInstanceName(*this);

  outlet->setPropagate(true);

  init();
}

void ProcessModel::init()
{
  inlet->setCustomData("In");
  outlet->setCustomData("Out");
  m_inlets.push_back(inlet.get());
  m_outlets.push_back(outlet.get());

  m_graph = std::make_unique<TimenodeGraph>(*this);
}

bool ProcessModel::hasCycles() const noexcept
{
  return m_graph->hasCycles();
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
  comments.clear();
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
  }
  for (auto& cmt : comments)
  {
    cmt.setDate(cmt.date() * scale);
  }

  for (auto& interval : intervals)
  {
    interval.setStartDate(interval.date() * scale);
    // Note : scale the min / max.

    auto newdur = interval.duration.defaultDuration() * scale;
    IntervalDurations::Algorithms::scaleAllDurations(interval, newdur);

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
  return; // Disabled by Asana
}

void ProcessModel::startExecution() { }

void ProcessModel::stopExecution()
{
  for (EventModel& ev : events)
  {
    ev.setStatus(ExecutionStatus::Editing, *this);
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
      objects.append(elt);
  });
  return objects;
}

template <typename InputVec, typename OutputVec>
static void copySelected(const InputVec& in, OutputVec& out)
{
  for (const auto& elt : in)
  {
    if (elt.selection.get())
      out.append(elt);
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

const QVector<Id<IntervalModel>>
intervalsBeforeTimeSync(const Scenario::ProcessModel& scenar, const Id<TimeSyncModel>& timeSyncId)
{
  QVector<Id<IntervalModel>> cstrs;
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

TimeVal ProcessModel::contentDuration() const noexcept
{
  TimeVal max_tn_pos = TimeVal::zero();
  for (TimeSyncModel& t : timeSyncs)
  {
    if (t.date() > max_tn_pos)
      max_tn_pos = t.date();
  }
  return max_tn_pos;
}

void ProcessModel::ancestorStartDateChanged()
{
  for(auto& itv : intervals)
    itv.ancestorStartDateChanged();
}

void ProcessModel::ancestorTempoChanged()
{
  for(auto& itv : intervals)
    itv.ancestorTempoChanged();
}
}

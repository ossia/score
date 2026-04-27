// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequenceModel.hpp"

#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationProcessMetadata.hpp>

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <State/Message.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <Process/TimeValue.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Sequence::SequenceModel)

namespace Sequence
{

// Helper: create a flat linear automation curve at a given normalized Y in [0,1]
static void initFlatCurve(Curve::Model& curve, double normY)
{
  auto seg = new Curve::LinearSegment(Id<Curve::SegmentModel>(0), &curve);
  seg->setStart({0.0, normY});
  seg->setEnd({1.0, normY});
  curve.addSegment(seg);
}

// Compute the curve domain (min/max + start/end normalized in [0,1]) for an
// address by querying the device explorer. Returns std::nullopt if the address
// is unknown — the caller should fall back to a sensible default.
struct ResolvedDomain
{
  Curve::CurveDomain dom;
  double normY; // (dom.start - dom.min) / (dom.max - dom.min), clamped to [0,1]
};

static std::optional<ResolvedDomain>
resolveDomain(const score::DocumentContext& ctx, const State::AddressAccessor& addr)
{
  auto* devPlugin = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!devPlugin)
    return std::nullopt;

  auto* node = Device::try_getNodeFromAddress(devPlugin->rootNode(), addr.address);
  if(!node || !node->is<Device::AddressSettings>())
    return std::nullopt;

  const auto& settings = node->get<Device::AddressSettings>();
  Curve::CurveDomain dom{settings.domain.get(), settings.value};
  // Guarantee a non-degenerate domain so the normalization below is finite.
  if(dom.max - dom.min < 1e-9)
    dom.max = dom.min + 1.0;

  double normY = (dom.start - dom.min) / (dom.max - dom.min);
  if(normY < 0.0)
    normY = 0.0;
  if(normY > 1.0)
    normY = 1.0;
  return ResolvedDomain{dom, normY};
}

// Helper: create a TimeSync + Event + State chain at a given date
// Returns the IDs of the created objects
struct ISNode
{
  Id<Scenario::TimeSyncModel> tsId;
  Id<Scenario::EventModel> evId;
  Id<Scenario::StateModel> stId;
};

static ISNode createIS(
    const TimeVal& date, double ypos, const score::DocumentContext& ctx,
    SequenceModel& seq)
{
  ISNode node;
  node.tsId = getStrongId(seq.timeSyncs);
  node.evId = getStrongId(seq.events);
  node.stId = getStrongId(seq.states);

  auto ts = new Scenario::TimeSyncModel(node.tsId, date, &seq);
  seq.timeSyncs.add(ts);

  auto ev = new Scenario::EventModel(node.evId, node.tsId, date, &seq);
  seq.events.add(ev);
  ts->addEvent(node.evId);

  auto st = new Scenario::StateModel(node.stId, node.evId, ypos, ctx, &seq);
  seq.states.add(st);
  ev->addState(node.stId);

  return node;
}

// Helper: create an interval connecting two states
static Id<Scenario::IntervalModel> createSection(
    const Id<Scenario::StateModel>& startSt, const Id<Scenario::StateModel>& endSt,
    const score::DocumentContext& ctx, SequenceModel& seq)
{
  auto itvId = getStrongId(seq.intervals);
  auto& sst = seq.states.at(startSt);
  auto& est = seq.states.at(endSt);
  const auto& sev = seq.events.at(sst.eventId());
  const auto& eev = seq.events.at(est.eventId());

  auto itv = new Scenario::IntervalModel(itvId, 0.0, ctx, &seq);
  itv->setStartState(startSt);
  itv->setEndState(endSt);
  seq.intervals.add(itv);

  Scenario::SetNextInterval(sst, *itv);
  Scenario::SetPreviousInterval(est, *itv);

  auto dur = eev.date() - sev.date();
  itv->setStartDate(sev.date());
  Scenario::IntervalDurations::Algorithms::fixAllDurations(*itv, dur);

  return itvId;
}

SequenceModel::SequenceModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id,
    const score::DocumentContext& ctx, QObject* parent)
    : Process::ProcessModel{
        duration, id, Metadata<ObjectKey_k, SequenceModel>::get(), parent}
    , m_context{ctx}
{
  // Create start boundary node at time 0.
  // Boundary IDs follow the Scenario convention (start = 0, end = 1) so that
  // ScenarioInterface helpers and execution components agree on which TS / Event /
  // State is the "structural" entry / exit of the sub-scenario.
  m_startTimeSyncId = Scenario::startId<Scenario::TimeSyncModel>();
  m_startEventId = Scenario::startId<Scenario::EventModel>();
  const auto startStId = Scenario::startId<Scenario::StateModel>();

  auto startTs = new Scenario::TimeSyncModel(m_startTimeSyncId, TimeVal::zero(), this);
  startTs->setStartPoint(true);
  timeSyncs.add(startTs);

  auto startEv
      = new Scenario::EventModel(m_startEventId, m_startTimeSyncId, TimeVal::zero(), this);
  events.add(startEv);
  startTs->addEvent(m_startEventId);

  auto startSt = new Scenario::StateModel(startStId, m_startEventId, 0.02, ctx, this);
  states.add(startSt);
  startEv->addState(startStId);

  // Create end boundary node at duration
  m_endTimeSyncId = Scenario::endId<Scenario::TimeSyncModel>();
  m_endEventId = Scenario::endId<Scenario::EventModel>();
  const auto endStId = Scenario::endId<Scenario::StateModel>();

  auto endTs = new Scenario::TimeSyncModel(m_endTimeSyncId, duration, this);
  timeSyncs.add(endTs);

  auto endEv = new Scenario::EventModel(m_endEventId, m_endTimeSyncId, duration, this);
  events.add(endEv);
  endTs->addEvent(m_endEventId);

  auto endSt = new Scenario::StateModel(endStId, m_endEventId, 0.02, ctx, this);
  states.add(endSt);
  endEv->addState(endStId);

  // Create one section interval from start to end
  createSection(startStId, endStId, ctx, *this);

  // Derive parent boundary state IDs (not serialized, always reconstructed)
  if(auto* itv = qobject_cast<Scenario::IntervalModel*>(parent))
  {
    m_parentStartStateId = itv->startState();
    m_parentEndStateId = itv->endState();
  }

  metadata().setInstanceName(*this);
}

SequenceModel::~SequenceModel()
{
  intervals.clear();
  states.clear();
  events.clear();
  timeSyncs.clear();
}

// ---- Ordered helpers ----

QVector<Id<Scenario::TimeSyncModel>> SequenceModel::orderedTimeSyncs() const
{
  // Sort by date
  QVector<std::pair<TimeVal, Id<Scenario::TimeSyncModel>>> pairs;
  pairs.reserve(static_cast<int>(timeSyncs.size()));
  for(const auto& ts : timeSyncs)
    pairs.push_back({ts.date(), ts.id()});
  std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });

  QVector<Id<Scenario::TimeSyncModel>> result;
  result.reserve(pairs.size());
  for(const auto& p : pairs)
    result.push_back(p.second);
  return result;
}

QVector<Id<Scenario::IntervalModel>> SequenceModel::orderedIntervals() const
{
  // Sort by start date
  QVector<std::pair<TimeVal, Id<Scenario::IntervalModel>>> pairs;
  pairs.reserve(static_cast<int>(intervals.size()));
  for(const auto& itv : intervals)
    pairs.push_back({itv.date(), itv.id()});
  std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });

  QVector<Id<Scenario::IntervalModel>> result;
  result.reserve(pairs.size());
  for(const auto& p : pairs)
    result.push_back(p.second);
  return result;
}

// ---- Internal helpers ----

Id<Scenario::StateModel> SequenceModel::stateForTimeSync(
    const Id<Scenario::TimeSyncModel>& tsId) const
{
  const auto& ts = timeSyncs.at(tsId);
  SCORE_ASSERT(!ts.events().empty());
  const auto& ev = events.at(ts.events().front());
  SCORE_ASSERT(!ev.states().empty());
  return ev.states().front();
}

std::optional<Id<Scenario::IntervalModel>> SequenceModel::intervalBefore(
    const Id<Scenario::TimeSyncModel>& tsId) const
{
  auto stId = stateForTimeSync(tsId);
  const auto& st = states.at(stId);
  if(st.previousInterval())
    return *st.previousInterval();
  return std::nullopt;
}

std::optional<Id<Scenario::IntervalModel>> SequenceModel::intervalAfter(
    const Id<Scenario::TimeSyncModel>& tsId) const
{
  auto stId = stateForTimeSync(tsId);
  const auto& st = states.at(stId);
  if(st.nextInterval())
    return *st.nextInterval();
  return std::nullopt;
}

// ---- Namespace management ----

void SequenceModel::addParameter(
    const State::AddressAccessor& addr, const ossia::value& /*currentVal*/)
{
  if(m_namespace.contains(addr))
    return;
  m_namespace.append(addr);

  // Look up the actual current value + domain in the device explorer.
  // If absent (offline device, address removed), fall back to a flat midpoint.
  auto resolved = resolveDomain(m_context, addr);
  const double normY = resolved ? resolved->normY : 0.5;
  const double minVal = resolved ? resolved->dom.min : 0.0;
  const double maxVal = resolved ? resolved->dom.max : 1.0;

  // Create a flat automation at the resolved value in every section.
  for(auto& itv : intervals)
  {
    auto autoId = getStrongId(itv.processes);
    auto* automation
        = new Automation::ProcessModel(itv.duration.defaultDuration(), autoId, &itv);
    automation->setAddress(addr);
    automation->setMin(minVal);
    automation->setMax(maxVal);
    initFlatCurve(automation->curve(), normY);
    Scenario::AddProcess(itv, automation);
  }

  namespaceChanged();
}

void SequenceModel::removeParameter(const State::AddressAccessor& addr)
{
  m_namespace.removeAll(addr);

  // Remove automations for this address from all section intervals
  for(auto& itv : intervals)
  {
    for(const auto& proc : itv.processes)
    {
      if(auto* auto_proc = qobject_cast<const Automation::ProcessModel*>(&proc))
      {
        if(auto_proc->address() == addr)
        {
          Scenario::RemoveProcess(itv, proc.id());
          break;
        }
      }
    }
  }

  namespaceChanged();
}

void SequenceModel::mergeNamespace(const QList<State::AddressAccessor>& addrs)
{
  for(const auto& addr : addrs)
  {
    if(!m_namespace.contains(addr))
      addParameter(addr, ossia::value{});
  }
}

// ---- Linear structure mutation ----

// Section helper that uses caller-supplied IDs. The IDs are passed through
// directly; no getStrongId() calls so that the same IDs survive across
// undo/redo cycles.
static Id<Scenario::IntervalModel> createSectionWithId(
    const Id<Scenario::IntervalModel>& itvId,
    const Id<Scenario::StateModel>& startSt, const Id<Scenario::StateModel>& endSt,
    const score::DocumentContext& ctx, SequenceModel& seq)
{
  auto& sst = seq.states.at(startSt);
  auto& est = seq.states.at(endSt);
  const auto& sev = seq.events.at(sst.eventId());
  const auto& eev = seq.events.at(est.eventId());

  auto itv = new Scenario::IntervalModel(itvId, 0.0, ctx, &seq);
  itv->setStartState(startSt);
  itv->setEndState(endSt);
  seq.intervals.add(itv);

  Scenario::SetNextInterval(sst, *itv);
  Scenario::SetPreviousInterval(est, *itv);

  auto dur = eev.date() - sev.date();
  itv->setStartDate(sev.date());
  Scenario::IntervalDurations::Algorithms::fixAllDurations(*itv, dur);

  return itvId;
}

SequenceModel::AppendedSection SequenceModel::appendSection(const TimeVal& duration)
{
  // Single source of truth: pre-allocate fresh IDs and delegate to the
  // caller-supplied-IDs variant. Keeps direct callers (tests, programmatic
  // scenarios) working without needing to know about ID stability rules.
  AppendedSection info;
  info.prevEndTimeSyncId = m_endTimeSyncId;
  info.prevEndEventId = m_endEventId;
  info.prevDuration = this->duration();
  info.newEndTimeSyncId = getStrongId(timeSyncs);
  info.newEndEventId = getStrongId(events);
  info.newEndStateId = getStrongId(states);
  info.newIntervalId = getStrongId(intervals);

  appendSectionWithIds(info, duration);
  return info;
}

void SequenceModel::appendSectionWithIds(
    const AppendedSection& info, const TimeVal& duration)
{
  // The current end IS stays at its date, becoming an intermediate IS.
  // A new end IS is created at current_end_date + duration.
  const auto& endTs = timeSyncs.at(m_endTimeSyncId);
  const TimeVal oldEndDate = endTs.date();
  const TimeVal newEndDate = oldEndDate + duration;

  // Create new end IS using pre-allocated IDs
  auto newTs = new Scenario::TimeSyncModel(info.newEndTimeSyncId, newEndDate, this);
  timeSyncs.add(newTs);

  auto newEv
      = new Scenario::EventModel(info.newEndEventId, info.newEndTimeSyncId, newEndDate, this);
  events.add(newEv);
  newTs->addEvent(info.newEndEventId);

  auto newSt = new Scenario::StateModel(
      info.newEndStateId, info.newEndEventId, 0.02, m_context, this);
  states.add(newSt);
  newEv->addState(info.newEndStateId);

  // Create interval from old end boundary to new end boundary
  const auto oldEndStId = stateForTimeSync(m_endTimeSyncId);
  createSectionWithId(
      info.newIntervalId, oldEndStId, info.newEndStateId, m_context, *this);

  // Create flat automations in the new interval for all namespace parameters,
  // initialized to the actual current device value (same logic as addParameter).
  auto& newItv = intervals.at(info.newIntervalId);
  for(const auto& addr : m_namespace)
  {
    auto resolved = resolveDomain(m_context, addr);
    const double normY = resolved ? resolved->normY : 0.5;
    const double minVal = resolved ? resolved->dom.min : 0.0;
    const double maxVal = resolved ? resolved->dom.max : 1.0;

    auto autoId = getStrongId(newItv.processes);
    auto* automation = new Automation::ProcessModel(
        newItv.duration.defaultDuration(), autoId, &newItv);
    automation->setAddress(addr);
    automation->setMin(minVal);
    automation->setMax(maxVal);
    initFlatCurve(automation->curve(), normY);
    Scenario::AddProcess(newItv, automation);
  }

  // Update end pointers
  m_endTimeSyncId = info.newEndTimeSyncId;
  m_endEventId = info.newEndEventId;

  // Update process duration
  setDuration(newEndDate);

  structureChanged();
}

void SequenceModel::undoAppendSection(const AppendedSection& info)
{
  // Disconnect the prev-end state's next-interval link before removal
  auto& prevEndSt = states.at(stateForTimeSync(info.prevEndTimeSyncId));
  prevEndSt.setNextInterval(std::nullopt);

  // Remove the new state's previous-interval link too
  auto& newEndSt = states.at(info.newEndStateId);
  newEndSt.setPreviousInterval(std::nullopt);

  // Remove the interval connecting prev-end to new-end
  intervals.remove(info.newIntervalId);

  // Remove the new-end entities
  states.remove(info.newEndStateId);
  events.remove(info.newEndEventId);
  timeSyncs.remove(info.newEndTimeSyncId);

  // Restore end pointers
  m_endTimeSyncId = info.prevEndTimeSyncId;
  m_endEventId = info.prevEndEventId;

  setDuration(info.prevDuration);
  structureChanged();
}

void SequenceModel::moveIS(
    const Id<Scenario::TimeSyncModel>& tsId, const TimeVal& newDate)
{
  // Don't move start or end
  SCORE_ASSERT(tsId != m_startTimeSyncId);
  SCORE_ASSERT(tsId != m_endTimeSyncId);

  auto& ts = timeSyncs.at(tsId);
  const TimeVal oldDate = ts.date();
  ts.setDate(newDate);

  const auto stId = stateForTimeSync(tsId);
  auto& ev = events.at(states.at(stId).eventId());
  ev.setDate(newDate);

  // Resize left-adjacent interval
  if(auto leftItvId = intervalBefore(tsId))
  {
    auto& leftItv = intervals.at(*leftItvId);
    const auto& startEv
        = events.at(states.at(leftItv.startState()).eventId());
    auto newDur = newDate - startEv.date();
    Scenario::IntervalDurations::Algorithms::fixAllDurations(leftItv, newDur);
  }

  // Resize right-adjacent interval
  if(auto rightItvId = intervalAfter(tsId))
  {
    auto& rightItv = intervals.at(*rightItvId);
    const auto& endEv = events.at(states.at(rightItv.endState()).eventId());
    auto newDur = endEv.date() - newDate;
    rightItv.setStartDate(newDate);
    Scenario::IntervalDurations::Algorithms::fixAllDurations(rightItv, newDur);
  }

  structureChanged();
}

// ---- IS value management ----

namespace
{
// Normalize an ossia::value into [0,1] against the resolved domain.
// Falls back to 0.5 if the value can't be reduced to a finite scalar.
double normalizeValue(const ossia::value& val, double minVal, double maxVal)
{
  const float v = ossia::convert<float>(val);
  const double range = maxVal - minVal;
  if(range < 1e-9)
    return 0.5;
  double norm = (double(v) - minVal) / range;
  if(norm < 0.0)
    norm = 0.0;
  if(norm > 1.0)
    norm = 1.0;
  return norm;
}
}

void SequenceModel::syncAutomationEndpoints(
    const Id<Scenario::TimeSyncModel>& tsId, const State::AddressAccessor& addr,
    const ossia::value& val)
{
  auto resolved = resolveDomain(m_context, addr);
  if(!resolved)
    return;

  const double normY
      = normalizeValue(val, resolved->dom.min, resolved->dom.max);

  // Update end point of left-adjacent automation (curve x=1)
  if(auto leftItvId = intervalBefore(tsId))
  {
    auto& leftItv = intervals.at(*leftItvId);
    for(auto& proc : leftItv.processes)
    {
      if(auto* auto_proc = qobject_cast<Automation::ProcessModel*>(&proc))
      {
        if(auto_proc->address() == addr)
        {
          auto& curve = auto_proc->curve();
          auto segs = curve.sortedSegments();
          if(!segs.empty())
          {
            auto* lastSeg = segs.back();
            lastSeg->setEnd({1.0, normY});
            curve.changed();
          }
          break;
        }
      }
    }
  }

  // Update start point of right-adjacent automation (curve x=0)
  if(auto rightItvId = intervalAfter(tsId))
  {
    auto& rightItv = intervals.at(*rightItvId);
    for(auto& proc : rightItv.processes)
    {
      if(auto* auto_proc = qobject_cast<Automation::ProcessModel*>(&proc))
      {
        if(auto_proc->address() == addr)
        {
          auto& curve = auto_proc->curve();
          auto segs = curve.sortedSegments();
          if(!segs.empty())
          {
            auto* firstSeg = segs.front();
            firstSeg->setStart({0.0, normY});
            curve.changed();
          }
          break;
        }
      }
    }
  }
}

void SequenceModel::setISValue(
    const Id<Scenario::TimeSyncModel>& tsId, const State::AddressAccessor& addr,
    const ossia::value& val)
{
  // 1. Persist the user-supplied value on the IS state as a flat message.
  //    The MessageItemModel is what the inspector and the executor read.
  if(auto* state = findState(stateForTimeSync(tsId)))
  {
    State::MessageList lst{State::Message{addr, val}};
    Scenario::updateModelWithMessageList(state->messages(), std::move(lst));
  }

  // 2. Pull the new value into the adjacent automations' endpoints.
  syncAutomationEndpoints(tsId, addr, val);

  // 3. Freeze propagation: walk forward through consecutive frozen IS so that
  //    each one stores the same value and its automations stay flat.
  auto ordered = orderedTimeSyncs();
  int tsIdx = ordered.indexOf(tsId);
  if(tsIdx < 0)
    return;

  for(int i = tsIdx + 1; i < ordered.size(); ++i)
  {
    const auto& nextTsId = ordered[i];
    if(!isParamFrozen(nextTsId, addr))
      break;
    if(auto* state = findState(stateForTimeSync(nextTsId)))
    {
      State::MessageList lst{State::Message{addr, val}};
      Scenario::updateModelWithMessageList(state->messages(), std::move(lst));
    }
    syncAutomationEndpoints(nextTsId, addr, val);
  }
}

void SequenceModel::freezeISParam(
    const Id<Scenario::TimeSyncModel>& tsId, const State::AddressAccessor& addr,
    bool frozen)
{
  if(frozen)
    m_frozenParams[tsId].insert(addr);
  else
    m_frozenParams[tsId].remove(addr);
}

bool SequenceModel::isParamFrozen(
    const Id<Scenario::TimeSyncModel>& tsId,
    const State::AddressAccessor& addr) const
{
  auto it = m_frozenParams.find(tsId);
  if(it == m_frozenParams.end())
    return false;
  return it->contains(addr);
}

void SequenceModel::rebuildAutomations(const State::AddressAccessor& addr)
{
  for(auto& itv : intervals)
  {
    // Check if an automation for addr already exists
    bool found = false;
    for(const auto& proc : itv.processes)
    {
      if(auto* auto_proc = qobject_cast<const Automation::ProcessModel*>(&proc))
      {
        if(auto_proc->address() == addr)
        {
          found = true;
          break;
        }
      }
    }

    if(!found)
    {
      auto resolved = resolveDomain(m_context, addr);
      const double normY = resolved ? resolved->normY : 0.5;
      const double minVal = resolved ? resolved->dom.min : 0.0;
      const double maxVal = resolved ? resolved->dom.max : 1.0;

      auto autoId = getStrongId(itv.processes);
      auto* automation = new Automation::ProcessModel(
          itv.duration.defaultDuration(), autoId, &itv);
      automation->setAddress(addr);
      automation->setMin(minVal);
      automation->setMax(maxVal);
      initFlatCurve(automation->curve(), normY);
      Scenario::AddProcess(itv, automation);
    }
  }
}

// ---- ProcessModel overrides ----

TimeVal SequenceModel::contentDuration() const noexcept
{
  TimeVal max = TimeVal::zero();
  for(const auto& ts : timeSyncs)
  {
    if(ts.date() > max)
      max = ts.date();
  }
  return max;
}

void SequenceModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  if(duration() == TimeVal::zero())
    return;

  double scale = newDuration / duration();

  for(auto& ts : timeSyncs)
    ts.setDate(ts.date() * scale);

  for(auto& ev : events)
    ev.setDate(ev.date() * scale);

  for(auto& itv : intervals)
  {
    itv.setStartDate(itv.date() * scale);
    auto newdur = itv.duration.defaultDuration() * scale;
    Scenario::IntervalDurations::Algorithms::scaleAllDurations(itv, newdur);
    for(auto& proc : itv.processes)
      proc.setParentDuration(ExpandMode::Scale, newdur);
  }

  setDuration(newDuration);
}

// Last-section-absorbs-slack strategy when the parent interval grows or shrinks
// in non-Scale mode. Mirrors how Nodal::Model defers to its child processes
// instead of silently keeping their old extents.
static Scenario::IntervalModel*
findLastSectionInterval(const score::EntityMap<Scenario::IntervalModel>& intervals)
{
  Scenario::IntervalModel* last = nullptr;
  TimeVal lastDate = TimeVal::zero();
  for(auto& itv : intervals)
  {
    if(!last || itv.date() >= lastDate)
    {
      lastDate = itv.date();
      last = &const_cast<Scenario::IntervalModel&>(itv);
    }
  }
  return last;
}

void SequenceModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  const auto cur = duration();
  if(newDuration <= cur)
  {
    setDuration(newDuration);
    return;
  }

  auto* lastItv = findLastSectionInterval(intervals);
  if(!lastItv)
  {
    setDuration(newDuration);
    return;
  }

  const auto extra = newDuration - cur;

  // Move end boundary forward by the extra delta
  timeSyncs.at(m_endTimeSyncId).setDate(timeSyncs.at(m_endTimeSyncId).date() + extra);
  events.at(m_endEventId).setDate(events.at(m_endEventId).date() + extra);
  states.at(stateForTimeSync(m_endTimeSyncId)); // touch — keep symmetry; state has no date

  // Stretch the last section
  const auto newSecDur = lastItv->duration.defaultDuration() + extra;
  Scenario::IntervalDurations::Algorithms::fixAllDurations(*lastItv, newSecDur);
  for(auto& proc : lastItv->processes)
    proc.setParentDuration(ExpandMode::GrowShrink, newSecDur);

  setDuration(newDuration);
}

void SequenceModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  const auto cur = duration();
  if(newDuration >= cur)
  {
    setDuration(newDuration);
    return;
  }

  auto* lastItv = findLastSectionInterval(intervals);
  if(!lastItv)
  {
    setDuration(newDuration);
    return;
  }

  const auto delta = cur - newDuration;
  const auto curSecDur = lastItv->duration.defaultDuration();
  // Clamp the last section to a strictly-positive minimum so the executor
  // does not see a zero-length interval. If the request would underflow
  // (the sum of fixed earlier sections already exceeds newDuration) we
  // clamp the last section and propagate the clamped delta upwards.
  const auto minSecDur = TimeVal::fromMsecs(1.);
  auto newSecDur = curSecDur - delta;
  if(newSecDur < minSecDur)
    newSecDur = minSecDur;
  const auto actualDelta = curSecDur - newSecDur;
  const auto actualDuration = cur - actualDelta;

  timeSyncs.at(m_endTimeSyncId).setDate(timeSyncs.at(m_endTimeSyncId).date() - actualDelta);
  events.at(m_endEventId).setDate(events.at(m_endEventId).date() - actualDelta);

  Scenario::IntervalDurations::Algorithms::fixAllDurations(*lastItv, newSecDur);
  for(auto& proc : lastItv->processes)
    proc.setParentDuration(ExpandMode::GrowShrink, newSecDur);

  setDuration(actualDuration);
}

Selection SequenceModel::selectableChildren() const noexcept
{
  Selection s;
  for(auto& itv : intervals)
    s.append(&itv);
  for(auto& ev : events)
    s.append(&ev);
  for(auto& ts : timeSyncs)
    s.append(&ts);
  for(auto& st : states)
    s.append(&st);
  return s;
}

Selection SequenceModel::selectedChildren() const noexcept
{
  Selection s;
  for(auto& itv : intervals)
    if(itv.selection.get())
      s.append(&itv);
  for(auto& ev : events)
    if(ev.selection.get())
      s.append(&ev);
  for(auto& ts : timeSyncs)
    if(ts.selection.get())
      s.append(&ts);
  for(auto& st : states)
    if(st.selection.get())
      s.append(&st);
  return s;
}

void SequenceModel::setSelection(const Selection& s) const noexcept
{
  for(auto& itv : intervals)
    itv.selection.set(s.contains(&itv));
  for(auto& ev : events)
    ev.selection.set(s.contains(&ev));
  for(auto& ts : timeSyncs)
    ts.selection.set(s.contains(&ts));
  for(auto& st : states)
    st.selection.set(s.contains(&st));
}

} // namespace Sequence

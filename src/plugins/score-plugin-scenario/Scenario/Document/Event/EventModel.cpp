// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "EventModel.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <State/Expression.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>

#include <QObject>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::EventModel)
namespace Scenario
{
EventModel::EventModel(
    const Id<EventModel>& id,
    const Id<TimeSyncModel>& timesync,
    const TimeVal& date,
    QObject* parent)
    : Entity{id, Metadata<ObjectKey_k, EventModel>::get(), parent}
    , m_timeSync{timesync}
    , m_condition{}
    , m_date{date}
    , m_offset{OffsetBehavior::True}
{
  metadata().setInstanceName(*this);
  metadata().setColor(&score::Skin::Emphasis4);
}

void EventModel::changeTimeSync(const Id<TimeSyncModel>& elt)
{
  auto old = m_timeSync;
  m_timeSync = elt;
  timeSyncChanged(old, elt);
}

const TimeVal& EventModel::date() const noexcept
{
  return m_date;
}

void EventModel::setDate(const TimeVal& date)
{
  if (m_date != date)
  {
    m_date = date;
    dateChanged(m_date);
  }
}

void EventModel::setStatus(
    ExecutionStatus status,
    const ScenarioInterface& scenar)
{
  if (m_status.get() == status)
    return;

  m_status.set(status);
  statusChanged(status);

  for (auto& state : m_states)
  {
    scenar.state(state).setStatus(status);
  }
}

void EventModel::setOffsetBehavior(OffsetBehavior f)
{
  if (m_offset != f)
  {
    m_offset = f;
    offsetBehaviorChanged(f);
  }
}

const QBrush& EventModel::color(const Process::Style& skin) const noexcept
{
  if (m_status.get() == ExecutionStatus::Editing)
    return metadata().getColor().getBrush();
  else
    return m_status.eventStatusColor(skin);
}

void EventModel::translate(const TimeVal& deltaTime)
{
  setDate(m_date + deltaTime);
}

ExecutionStatus EventModel::status() const noexcept
{
  return m_status.get();
}

void EventModel::addState(const Id<StateModel>& ds)
{
  if (ossia::contains(m_states, ds))
    return;
  m_states.push_back(ds);
  statesChanged();
}

void EventModel::removeState(const Id<StateModel>& ds)
{
  auto it = ossia::find(m_states, ds);
  if (it != m_states.end())
  {
    m_states.erase(it);
    statesChanged();
  }
}

void EventModel::clearStates()
{
  m_states.clear();
  statesChanged();
}

const EventModel::StateIdVec& EventModel::states() const noexcept
{
  return m_states;
}

const State::Expression& EventModel::condition() const noexcept
{
  return m_condition;
}

OffsetBehavior EventModel::offsetBehavior() const noexcept
{
  return m_offset;
}

void EventModel::setCondition(const State::Expression& arg)
{
  if (m_condition != arg)
  {
    m_condition = arg;
    conditionChanged(arg);
  }
}
}

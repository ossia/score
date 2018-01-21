// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>
#include <QObject>
#include <score/document/DocumentInterface.hpp>

#include <QPoint>

#include "EventModel.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <State/Expression.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>

namespace Scenario
{
EventModel::EventModel(
    const Id<EventModel>& id,
    const Id<TimeSyncModel>& timesync,
    const VerticalExtent& extent,
    const TimeVal& date,
    QObject* parent)
    : Entity{id, Metadata<ObjectKey_k, EventModel>::get(), parent}
    , m_timeSync{timesync}
    , m_condition{}
    , m_extent{extent}
    , m_date{date}
    , m_offset{OffsetBehavior::True}
{
  metadata().setInstanceName(*this);
  metadata().setColor(ScenarioStyle::instance().EventDefault);
}

VerticalExtent EventModel::extent() const
{
  return m_extent;
}

void EventModel::setExtent(const VerticalExtent& extent)
{
  if (extent != m_extent)
  {
    m_extent = extent;
    emit extentChanged(m_extent);
  }
}

const TimeVal& EventModel::date() const
{
  return m_date;
}

void EventModel::setDate(const TimeVal& date)
{
  if (m_date != date)
  {
    m_date = date;
    emit dateChanged(m_date);
  }
}

void EventModel::setStatus(
    ExecutionStatus status,
    const ScenarioInterface& scenar)
{
  if (m_status.get() == status)
    return;

  m_status.set(status);
  emit statusChanged(status);

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
    emit offsetBehaviorChanged(f);
  }
}

void EventModel::translate(const TimeVal& deltaTime)
{
  setDate(m_date + deltaTime);
}

ExecutionStatus EventModel::status() const
{
  return m_status.get();
}

// TODO Maybe remove the need for this by passing to the scenario instead ?

void EventModel::addState(const Id<StateModel>& ds)
{
  if (ossia::contains(m_states, ds))
    return;
  m_states.push_back(ds);
  emit statesChanged();
}

void EventModel::removeState(const Id<StateModel>& ds)
{
  auto it = ossia::find(m_states, ds);
  if (it != m_states.end())
  {
    m_states.erase(it);
    emit statesChanged();
  }
}

void EventModel::clearStates()
{
  m_states.clear();
  emit statesChanged();
}

const EventModel::StateIdVec& EventModel::states() const
{
  return m_states;
}

const State::Expression& EventModel::condition() const
{
  return m_condition;
}

OffsetBehavior EventModel::offsetBehavior() const
{
  return m_offset;
}

void EventModel::setCondition(const State::Expression& arg)
{
  if (m_condition != arg)
  {
    m_condition = arg;
    emit conditionChanged(arg);
  }
}
}

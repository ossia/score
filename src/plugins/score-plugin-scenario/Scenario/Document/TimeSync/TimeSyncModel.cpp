// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeSyncModel.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::TimeSyncModel)
namespace Scenario
{
TimeSyncModel::TimeSyncModel(const Id<TimeSyncModel>& id, const TimeVal& date, QObject* parent)
    : Entity{id, Metadata<ObjectKey_k, TimeSyncModel>::get(), parent}, m_date{date}
{
  m_expression = State::defaultFalseExpression();
  metadata().setInstanceName(*this);
  metadata().setColor(&score::Skin::Gray);
}

void TimeSyncModel::addEvent(const Id<EventModel>& eventId)
{
  m_events.push_back(eventId);

  auto scenar = dynamic_cast<ScenarioInterface*>(parent());
  if (scenar)
  {
    // There may be no scenario when we are cloning without a parent.
    // TODO this addEvent should be in an outside algorithm.
    auto& theEvent = scenar->event(eventId);
    theEvent.changeTimeSync(this->id());
  }

  newEvent(eventId);
}

bool TimeSyncModel::removeEvent(const Id<EventModel>& eventId)
{
  auto it = ossia::find(m_events, eventId);
  if (it != m_events.end())
  {
    m_events.erase(it);
    eventRemoved(eventId);
    return true;
  }

  return false;
}

void TimeSyncModel::clearEvents()
{
  auto ev = m_events;
  m_events.clear();
  for (const auto& e : ev)
    eventRemoved(e);
}

const TimeVal& TimeSyncModel::date() const noexcept
{
  return m_date;
}

void TimeSyncModel::setDate(const TimeVal& date)
{
  m_date = date;
  dateChanged(m_date);
}

const TimeSyncModel::EventIdVec& TimeSyncModel::events() const noexcept
{
  return m_events;
}

void TimeSyncModel::setEvents(const TimeSyncModel::EventIdVec& events)
{
  m_events = events;
}

void TimeSyncModel::setExpression(const State::Expression& expression)
{
  if (m_expression == expression)
    return;
  m_expression = expression;
  triggerChanged(m_expression);
}

bool TimeSyncModel::active() const noexcept
{
  return m_active;
}

void TimeSyncModel::setActive(bool active)
{
  if (active == m_active)
    return;
  m_active = active;
  activeChanged();
}

bool TimeSyncModel::autotrigger() const noexcept
{
  return m_autotrigger;
}
void TimeSyncModel::setAutotrigger(bool a)
{
  if (a == m_autotrigger)
    return;
  m_autotrigger = a;
  autotriggerChanged(a);
}

bool TimeSyncModel::isStartPoint() const noexcept
{
  return m_startPoint;
}
void TimeSyncModel::setStartPoint(bool a)
{
  if (a == m_startPoint)
    return;
  m_startPoint = a;
  startPointChanged(a);
}

Control::musical_sync TimeSyncModel::musicalSync() const noexcept
{
  return m_musicalSync;
}

void TimeSyncModel::setMusicalSync(Control::musical_sync s)
{
  if (m_musicalSync != s)
  {
    m_musicalSync = s;
    musicalSyncChanged(s);
  }
}

void TimeSyncModel::setWaiting(bool b)
{
  m_waiting = b;
  waitingChanged(b);
}

bool TimeSyncModel::waiting() const noexcept
{
  return m_waiting;
}

}

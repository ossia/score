#include <Process/Style/ScenarioStyle.hpp>
#include <QtGlobal>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>

#include "TimeNodeModel.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
TimeNodeModel::TimeNodeModel(
    const Id<TimeNodeModel>& id,
    const VerticalExtent& extent,
    const TimeValue& date,
    QObject* parent)
    : Entity{id, Metadata<ObjectKey_k, TimeNodeModel>::get(), parent}
    , m_extent{extent}
    , m_date{date}
    , m_trigger{new TriggerModel{Id<TriggerModel>(0), this}}
{
  metadata().setInstanceName(*this);
  metadata().setColor(ScenarioStyle::instance().TimenodeDefault);
}

TimeNodeModel::TimeNodeModel(
    const TimeNodeModel& source, const Id<TimeNodeModel>& id, QObject* parent)
    : Entity{source, id, Metadata<ObjectKey_k, TimeNodeModel>::get(), parent}
    , m_extent{source.m_extent}
    , m_date{source.m_date}
    , m_events(source.m_events)
{
  metadata().setInstanceName(*this);
  m_trigger = new TriggerModel{Id<TriggerModel>(0), this};
  m_trigger->setExpression(source.trigger()->expression());
  m_trigger->setActive(source.trigger()->active());
}

void TimeNodeModel::addEvent(const Id<EventModel>& eventId)
{
  m_events.push_back(eventId);

  auto scenar = dynamic_cast<ScenarioInterface*>(parent());
  if (scenar)
  {
    // There may be no scenario when we are cloning without a parent.
    // TODO this addEvent should be in an outside algorithm.
    auto& theEvent = scenar->event(eventId);
    theEvent.changeTimeNode(this->id());
  }

  emit newEvent(eventId);
}

bool TimeNodeModel::removeEvent(const Id<EventModel>& eventId)
{
  if (m_events.indexOf(eventId) >= 0)
  {
    m_events.remove(m_events.indexOf(eventId));
    emit eventRemoved(eventId);
    return true;
  }

  return false;
}

const TimeValue& TimeNodeModel::date() const
{
  return m_date;
}

void TimeNodeModel::setDate(const TimeValue& date)
{
  m_date = date;
  emit dateChanged(m_date);
}

const QVector<Id<EventModel>>& TimeNodeModel::events() const
{
  return m_events;
}

void TimeNodeModel::setEvents(const QVector<Id<EventModel>>& events)
{
  m_events = events;
}

TriggerModel* TimeNodeModel::trigger() const
{
  return m_trigger;
}

QString TimeNodeModel::expression() const
{
  return m_trigger->expression().toString();
}

const VerticalExtent& TimeNodeModel::extent() const
{
  return m_extent;
}

void TimeNodeModel::setExtent(const VerticalExtent& extent)
{
  if (extent != m_extent)
  {
    m_extent = extent;
    emit extentChanged(m_extent);
  }
}

bool TimeNodeModel::hasTrigger() const
{
  return m_trigger->active();
}
}

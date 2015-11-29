#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <qglobal.h>

#include "Process/ModelMetadata.hpp"
#include "Process/TimeValue.hpp"
#include "Scenario/Document/VerticalExtent.hpp"
#include "Scenario/Process/ScenarioInterface.hpp"
#include "TimeNodeModel.hpp"
#include "iscore/document/DocumentInterface.hpp"
#include "iscore/tools/IdentifiedObject.hpp"
#include "iscore/tools/SettableIdentifier.hpp"

TimeNodeModel::TimeNodeModel(
        const Id<TimeNodeModel>& id,
        const VerticalExtent& extent,
        const TimeValue& date,
        QObject* parent):
    IdentifiedObject<TimeNodeModel> {id, "TimeNodeModel", parent},
    pluginModelList{iscore::IDocument::documentFromObject(parent), this},
    m_extent{extent},
    m_date{date},
    m_trigger{new TriggerModel{Id<TriggerModel>(0), this} }
{
    metadata.setName(QString("TimeNode.%1").arg(*this->id().val()));
    metadata.setLabel("TimeNode");
    metadata.setColor(ScenarioStyle::instance().TimenodeDefault);
}

TimeNodeModel::TimeNodeModel(
        const TimeNodeModel &source,
        const Id<TimeNodeModel>& id,
        QObject* parent):
    IdentifiedObject<TimeNodeModel> {id, "TimeNodeModel", parent},
    metadata{source.metadata},
    pluginModelList{source.pluginModelList, this},
    m_extent{source.m_extent},
    m_date{source.m_date},
    m_events(source.m_events)
{
    m_trigger = new TriggerModel{Id<TriggerModel>(0), this};
    m_trigger->setExpression(source.trigger()->expression());
    m_trigger->setActive(source.trigger()->active());
}

ScenarioInterface* TimeNodeModel::parentScenario() const
{
    return dynamic_cast<ScenarioInterface*>(parent());
}

void TimeNodeModel::addEvent(const Id<EventModel>& eventId)
{
    m_events.push_back(eventId);
    emit newEvent(eventId);

    auto scenar = parentScenario();
    if(!scenar)
        return;

    auto& theEvent = scenar->event(eventId);
    theEvent.changeTimeNode(this->id());
}

bool TimeNodeModel::removeEvent(const Id<EventModel>& eventId)
{
    if(m_events.indexOf(eventId) >= 0)
    {
        m_events.remove(m_events.indexOf(eventId));
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

const QVector<Id<EventModel> >& TimeNodeModel::events() const
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

const VerticalExtent& TimeNodeModel::extent() const
{
    return m_extent;
}

void TimeNodeModel::setExtent(const VerticalExtent &extent)
{
    // TODO if extent != ...
    m_extent = extent;
    emit extentChanged(m_extent);
}


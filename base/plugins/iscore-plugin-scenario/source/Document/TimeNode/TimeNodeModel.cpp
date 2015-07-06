#include "TimeNodeModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include "Document/Event/EventModel.hpp"

#include "Process/ScenarioModel.hpp"

TimeNodeModel::TimeNodeModel(
        const id_type<TimeNodeModel>& id,
        const VerticalExtent& extent,
        const TimeValue& date,
        QObject* parent):
    IdentifiedObject<TimeNodeModel> {id, "TimeNodeModel", parent},
    pluginModelList{iscore::IDocument::documentFromObject(parent), this},
    m_extent{extent},
    m_date{date}
{
    metadata.setName(QString("TimeNode.%1").arg(*this->id().val()));
    metadata.setLabel("TimeNode");
}

TimeNodeModel::TimeNodeModel(
        const TimeNodeModel &source,
        const id_type<TimeNodeModel>& id,
        QObject* parent):
    IdentifiedObject<TimeNodeModel> {id, "TimeNodeModel", parent},
    pluginModelList{source.pluginModelList, this},
    m_extent{source.m_extent},
    m_date{source.m_date},
    m_events(source.m_events)
{
    metadata.setName(QString("TimeNode.%1").arg(*this->id().val()));
    metadata.setLabel("TimeNode");
}

ScenarioModel* TimeNodeModel::parentScenario() const
{
    return dynamic_cast<ScenarioModel*>(parent());
}

void TimeNodeModel::addEvent(const id_type<EventModel>& eventId)
{
    m_events.push_back(eventId);
    emit newEvent(eventId);

    if(parentScenario())
    {
        auto& theEvent = parentScenario()->event(eventId);
        theEvent.changeTimeNode(this->id());
    }
}

bool TimeNodeModel::removeEvent(const id_type<EventModel>& eventId)
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

const QVector<id_type<EventModel> >& TimeNodeModel::events() const
{
    return m_events;
}

void TimeNodeModel::setEvents(const QVector<id_type<EventModel>>& events)
{
    m_events = events;
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


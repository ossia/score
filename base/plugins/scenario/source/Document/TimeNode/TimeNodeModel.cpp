#include "TimeNodeModel.hpp"
#include <iscore/document/DocumentInterface.hpp>

TimeNodeModel::TimeNodeModel(
        const id_type<TimeNodeModel>& id,
        const TimeValue& date,
        double ypos,
        QObject* parent):
    IdentifiedObject<TimeNodeModel> {id, "TimeNodeModel", parent},
    m_pluginModelList{new iscore::ElementPluginModelList{iscore::IDocument::documentFromObject(parent), this}},
    m_date{date},
    m_y{ypos}
{
    metadata.setName(QString("TimeNode.%1").arg(*this->id().val()));
    metadata.setLabel("TimeNode");
}

TimeNodeModel::TimeNodeModel(
        TimeNodeModel* source,
        const id_type<TimeNodeModel>& id,
        QObject* parent):
    TimeNodeModel{id, source->date(), source->y(), parent}
{
    m_pluginModelList = new iscore::ElementPluginModelList{source->m_pluginModelList, this};
    m_events = source->m_events;
}

#include "Process/ScenarioModel.hpp"
ScenarioModel* TimeNodeModel::parentScenario() const
{
    return dynamic_cast<ScenarioModel*>(parent());
}

void TimeNodeModel::addEvent(const id_type<EventModel>& eventId)
{
    m_events.push_back(eventId);
    emit newEvent(eventId);
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
    emit dateChanged();
}

bool TimeNodeModel::isEmpty()
{
    return (m_events.size() == 0);
}
double TimeNodeModel::y() const
{
    return m_y;
}

void TimeNodeModel::setY(double y)
{
    m_y = y;
}
const QVector<id_type<EventModel> >& TimeNodeModel::events() const
{
    return m_events;
}

void TimeNodeModel::setEvents(const QVector<id_type<EventModel>>& events)
{
    m_events = events;
}

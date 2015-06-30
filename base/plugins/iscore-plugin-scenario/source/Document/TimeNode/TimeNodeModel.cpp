#include "TimeNodeModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include "Document/Event/EventModel.hpp"

#include "Process/ScenarioModel.hpp"

TimeNodeModel::TimeNodeModel(
        const id_type<TimeNodeModel>& id,
        const TimeValue& date,
        double ypos,
        QObject* parent):
    IdentifiedObject<TimeNodeModel> {id, "TimeNodeModel", parent},
    pluginModelList{iscore::IDocument::documentFromObject(parent), this},
    m_date{date},
    m_y{ypos}
{
    metadata.setName(QString("TimeNode.%1").arg(*this->id().val()));
    metadata.setLabel("TimeNode");
}

TimeNodeModel::TimeNodeModel(const TimeNodeModel &source,
        const id_type<TimeNodeModel>& id,
        QObject* parent):
    IdentifiedObject<TimeNodeModel> {id, "TimeNodeModel", parent},
    pluginModelList{source.pluginModelList, this},
    m_date{source.date()},
    m_y{source.y()},
    m_events(source.m_events),
    m_eventHasPreviousConstraint(source.m_eventHasPreviousConstraint)
{
    metadata.setName(QString("TimeNode.%1").arg(*this->id().val()));
    metadata.setLabel("TimeNode");

    checkIfPreviousConstraint();
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
        m_eventHasPreviousConstraint[eventId] = theEvent.hasPreviousConstraint();
        checkIfPreviousConstraint();
    }
}

bool TimeNodeModel::removeEvent(const id_type<EventModel>& eventId)
{
    if(m_events.indexOf(eventId) >= 0)
    {
        m_events.remove(m_events.indexOf(eventId));
        m_eventHasPreviousConstraint.remove(eventId);
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

bool TimeNodeModel::checkIfPreviousConstraint()
{
    for(bool ev : m_eventHasPreviousConstraint)
    {
        if(ev)
            return true;
    }
    return false;
}

void TimeNodeModel::previousConstraintsChanged(const id_type<EventModel>& ev, bool hasPrevCstr)
{
    m_eventHasPreviousConstraint[ev] = hasPrevCstr;
    emit timeNodeValid(checkIfPreviousConstraint());
}

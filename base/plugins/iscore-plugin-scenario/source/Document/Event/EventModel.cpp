#include "EventModel.hpp"
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Process/ScenarioModel.hpp>

#include <iscore/document/DocumentInterface.hpp>

EventModel::EventModel(
        const id_type<EventModel>& id,
        const id_type<TimeNodeModel>& timenode,
        const VerticalExtent &extent,
        const TimeValue &date,
        QObject* parent):
    IdentifiedObject<EventModel> {id, "EventModel", parent},
    pluginModelList{iscore::IDocument::documentFromObject(parent), this},
    m_timeNode{timenode},
    m_extent{extent},
    m_date{date}
{
    metadata.setName(QString("Event.%1").arg(*this->id().val()));
}

EventModel::EventModel(const EventModel& source,
                       const id_type<EventModel>& id,
                       QObject* parent) :
    IdentifiedObject<EventModel> {id, "EventModel", parent},
    pluginModelList{source.pluginModelList, this},
    m_timeNode{source.m_timeNode},
    m_states(source.m_states),
    m_condition{source.m_condition},
    m_trigger{source.m_trigger},
    m_extent{source.m_extent},
    m_date{source.m_date}
{
    metadata.setName(QString("Event.%1").arg(*this->id().val()));
}

void EventModel::changeTimeNode(const id_type<TimeNodeModel>& newTimeNodeId)
{
    m_timeNode = newTimeNodeId;
}

const id_type<TimeNodeModel>& EventModel::timeNode() const
{
    return m_timeNode;
}


VerticalExtent EventModel::extent() const
{
    return m_extent;
}

void EventModel::setExtent(const VerticalExtent &extent)
{
    // TODO add a check
    m_extent = extent;
    emit extentChanged(m_extent);
}

const TimeValue& EventModel::date() const
{
    return m_date;
}

void EventModel::setDate(const TimeValue& date)
{ //TODO ajuster la date avec celle du Timenode
    if (m_date != date)
    {
        m_date = date;
        emit dateChanged(m_date);
    }
}

void EventModel::setStatus(EventStatus status)
{
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged(status);
}

void EventModel::translate(const TimeValue& deltaTime)
{
    setDate(m_date + deltaTime);
}

EventStatus EventModel::status() const
{
    return m_status;
}

void EventModel::reset()
{
    setStatus(EventStatus::Editing);
}


// TODO Maybe remove the need for this by passing to the scenario instead ?
QString EventModel::prettyName()
{ return QObject::tr("Event"); }

ScenarioInterface* EventModel::parentScenario() const
{
    return dynamic_cast<ScenarioInterface*>(parent());
}

void EventModel::addState(const id_type<StateModel> &ds)
{
    m_states.append(ds);
    emit statesChanged();
}

void EventModel::removeState(const id_type<StateModel> &ds)
{
    m_states.removeOne(ds);
    emit statesChanged();
}

const QVector<id_type<StateModel> > &EventModel::states() const
{
    return m_states;
}



const QString& EventModel::condition() const
{
    return m_condition;
}

void EventModel::setCondition(const QString& arg)
{
    if(m_condition == arg)
    {
        return;
    }

    m_condition = arg;
    emit conditionChanged(arg);
}

const QString& EventModel::trigger() const
{
    return m_trigger;
}

void EventModel::setTrigger(const QString& trigger)
{
    if (m_trigger == trigger)
        return;

    m_trigger = trigger;
    emit triggerChanged(trigger);
}

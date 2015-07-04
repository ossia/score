#include "EventModel.hpp"
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Process/ScenarioModel.hpp>

#include <iscore/document/DocumentInterface.hpp>

EventModel::EventModel(
        const id_type<EventModel>& id,
        const id_type<TimeNodeModel>& timenode,
        double yPos,
        QObject* parent):
    IdentifiedObject<EventModel> {id, "EventModel", parent},
    pluginModelList{iscore::IDocument::documentFromObject(parent), this},
    m_timeNode{timenode}
{
    metadata.setName(QString("Event.%1").arg(*this->id().val()));
}

EventModel::EventModel(const EventModel& source,
                       const id_type<EventModel>& id,
                       QObject* parent) :
    IdentifiedObject<EventModel> {id, "EventModel", parent},
    pluginModelList{source.pluginModelList, this},
    m_timeNode{source.timeNode()},
    m_states(source.m_states),
    m_condition{source.m_condition},
    m_date{source.m_date},
    m_trigger{source.m_trigger}
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

const TimeValue& EventModel::date() const
{
    return m_date;
}

void EventModel::setDate(const TimeValue& date)
{
    if (m_date != date)
    {
        m_date = date;
        emit dateChanged();
    }
} //TODO ajuster la date avec celle du Timenode

void EventModel::translate(const TimeValue& deltaTime)
{
    setDate(m_date + deltaTime);
}

// TODO Maybe remove the need for this by passing to the scenario instead ?
QString EventModel::prettyName()
{ return QObject::tr("Event"); }

ScenarioModel* EventModel::parentScenario() const
{
    return dynamic_cast<ScenarioModel*>(parent());
}

const QString& EventModel::condition() const
{
    return m_condition;
}

void EventModel::addDisplayedState(const id_type<DisplayedStateModel> &ds)
{
    m_states.append(ds);
    emit statesChanged();
}

void EventModel::removeDisplayedState(const id_type<DisplayedStateModel> &ds)
{
    m_states.removeOne(ds);
    emit statesChanged();
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

void EventModel::setTrigger(const QString& trigger)
{
    if (m_trigger == trigger)
        return;

    m_trigger = trigger;
    emit triggerChanged(trigger);
}


const QString& EventModel::trigger() const
{
    return m_trigger;
}

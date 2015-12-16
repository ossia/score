#include <Process/Style/ScenarioStyle.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QObject>

#include <QPoint>

#include "EventModel.hpp"
#include <Process/ModelMetadata.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <State/Expression.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

EventModel::EventModel(
        const Id<EventModel>& id,
        const Id<TimeNodeModel>& timenode,
        const VerticalExtent &extent,
        const TimeValue &date,
        QObject* parent):
    IdentifiedObject<EventModel> {id, "EventModel", parent},
    pluginModelList{iscore::IDocument::documentContext(*parent), this},
    m_timeNode{timenode},
    m_extent{extent},
    m_date{date}
{
    metadata.setName(QString("Event.%1").arg(*this->id().val()));
    metadata.setColor(ScenarioStyle::instance().EventDefault);
}

EventModel::EventModel(const EventModel& source,
                       const Id<EventModel>& id,
                       QObject* parent) :
    IdentifiedObject<EventModel> {id, "EventModel", parent},
    metadata{source.metadata},
    pluginModelList{source.pluginModelList, this},
    m_timeNode{source.m_timeNode},
    m_states(source.m_states),
    m_condition{source.m_condition},
    m_extent{source.m_extent},
    m_date{source.m_date}
{
}






VerticalExtent EventModel::extent() const
{
    return m_extent;
}

void EventModel::setExtent(const VerticalExtent &extent)
{
    if(extent != m_extent)
    {
        m_extent = extent;
        emit extentChanged(m_extent);
    }
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
        emit dateChanged(m_date);
    }
}

void EventModel::setStatus(ExecutionStatus status)
{
    if (m_status.get() == status)
        return;

    m_status.set(status);
    emit statusChanged(status);

    auto scenar = dynamic_cast<ScenarioInterface*>(parent());
    ISCORE_ASSERT(scenar);

    for(auto& state : m_states)
    {
        scenar->state(state).setStatus(status);
    }
}

void EventModel::translate(const TimeValue& deltaTime)
{
    setDate(m_date + deltaTime);
}

ExecutionStatus EventModel::status() const
{
    return m_status.get();
}

void EventModel::reset()
{
    setStatus(ExecutionStatus::Editing);
}


// TODO Maybe remove the need for this by passing to the scenario instead ?
QString EventModel::description()
{ return QObject::tr("Event"); }

void EventModel::addState(const Id<StateModel> &ds)
{
    auto idx = m_states.indexOf(ds, 0);
    if(idx != -1)
        return;
    m_states.append(ds);
    emit statesChanged();
}

void EventModel::removeState(const Id<StateModel> &ds)
{
    auto idx = m_states.indexOf(ds, 0);
    if(idx != -1)
    {
        m_states.remove(idx);
        emit statesChanged();
    }
}

const QVector<Id<StateModel> > &EventModel::states() const
{
    return m_states;
}



const iscore::Condition& EventModel::condition() const
{
    return m_condition;
}

void EventModel::setCondition(const iscore::Condition& arg)
{
    if(m_condition != arg)
    {
        m_condition = arg;
        emit conditionChanged(arg);
    }
}

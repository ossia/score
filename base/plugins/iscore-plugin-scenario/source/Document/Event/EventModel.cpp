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
    m_timeNode{timenode},
    m_heightPercentage{yPos}
{
    metadata.setName(QString("Event.%1").arg(*this->id().val()));
}

EventModel::EventModel(const EventModel& source,
                       const id_type<EventModel>& id,
                       QObject* parent) :
    IdentifiedObject<EventModel> {id, "EventModel", parent},
    pluginModelList{source.pluginModelList, this},
    m_timeNode{source.timeNode()},
    m_previousConstraints(source.previousConstraints()),
    m_nextConstraints(source.nextConstraints()),
    m_heightPercentage{source.heightPercentage()},
    m_states(source.m_states),
    m_condition{source.m_condition},
    m_date{source.m_date},
    m_trigger{source.m_trigger}
{
    metadata.setName(QString("Event.%1").arg(*this->id().val()));
}

const QVector<id_type<ConstraintModel>>& EventModel::previousConstraints() const
{
    return m_previousConstraints;
}

const QVector<id_type<ConstraintModel>>& EventModel::nextConstraints() const
{
    return m_nextConstraints;
}

QVector<id_type<ConstraintModel> > EventModel::constraints() const
{
    return m_previousConstraints + m_nextConstraints;
}

void EventModel::addNextConstraint(const id_type<ConstraintModel>& constraint)
{
    m_nextConstraints.push_back(constraint);
    emit nextConstraintsChanged();
}

void EventModel::addPreviousConstraint(const id_type<ConstraintModel>& constraint)
{
    m_previousConstraints.push_back(constraint);
    emit previousConstraintsChanged();
}

template<typename Vec>
bool removeConstraint(Vec& constraints, const id_type<ConstraintModel>& constraintToDelete)
{
    auto index = constraints.indexOf(constraintToDelete);
    if(index >= 0)
    {
        constraints.remove(index);
        return true;
    }

    return false;
}

void EventModel::removeNextConstraint(const id_type<ConstraintModel>& constraintToDelete)
{
    removeConstraint(m_nextConstraints, constraintToDelete);
    emit nextConstraintsChanged();
}

void EventModel::removePreviousConstraint(const id_type<ConstraintModel>& constraintToDelete)
{
    removeConstraint(m_previousConstraints, constraintToDelete);
    emit previousConstraintsChanged();
}

void EventModel::changeTimeNode(const id_type<TimeNodeModel>& newTimeNodeId)
{
    m_timeNode = newTimeNodeId;
}

const id_type<TimeNodeModel>& EventModel::timeNode() const
{
    return m_timeNode;
}

double EventModel::heightPercentage() const
{
    return m_heightPercentage;
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

const iscore::StateList& EventModel::states() const
{
    return m_states;
}

void EventModel::replaceStates(const iscore::StateList& newStates)
{
    m_states = newStates;
}

void EventModel::addState(const iscore::State& state)
{
    m_states.append(state);
    emit localStatesChanged();
}

void EventModel::removeState(const iscore::State& s)
{
    m_states.removeOne(s);
    emit localStatesChanged();
}

void EventModel::addDisplayedState(const id_type<DisplayedStateModel> &ds)
{
    m_dispStates.append(ds);
}

void EventModel::setHeightPercentage(double arg)
{
    if(m_heightPercentage != arg)
    {
        m_heightPercentage = arg;
        emit heightPercentageChanged(arg);
    }
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

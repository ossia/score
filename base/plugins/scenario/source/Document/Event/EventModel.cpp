#include "EventModel.hpp"
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Process/ScenarioModel.hpp>

#include <iscore/document/DocumentInterface.hpp>

EventModel::EventModel(id_type<EventModel> id,
                       id_type<TimeNodeModel> timenode,
                       double yPos,
                       QObject* parent):
    IdentifiedObject<EventModel> {id, "EventModel", parent},
    m_pluginModelList{new iscore::ElementPluginModelList{iscore::IDocument::documentFromObject(parent), this}},
    m_timeNode{timenode},
    m_heightPercentage{yPos}
{
    metadata.setName(QString("Event.%1").arg(*this->id().val()));
}

EventModel::EventModel(EventModel* source,
                       id_type<EventModel> id,
                       QObject* parent) :
    EventModel {id,
                source->timeNode(),
                source->heightPercentage(),
                parent}
{
    m_pluginModelList = new iscore::ElementPluginModelList{source->m_pluginModelList, this};
    m_previousConstraints = source->previousConstraints();
    m_nextConstraints = source->nextConstraints();

    m_states = source->m_states;

    m_condition = source->condition();
    m_date = source->date();
}

const QVector<id_type<ConstraintModel>>& EventModel::previousConstraints() const
{
    return m_previousConstraints;
}

const QVector<id_type<ConstraintModel>>& EventModel::nextConstraints() const
{
    return m_nextConstraints;
}

QVector<id_type<ConstraintModel> > EventModel::constraints()
{
    return m_previousConstraints + m_nextConstraints;
}

void EventModel::addNextConstraint(id_type<ConstraintModel> constraint)
{
    m_nextConstraints.push_back(constraint);
}

void EventModel::addPreviousConstraint(id_type<ConstraintModel> constraint)
{
    m_previousConstraints.push_back(constraint);
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

bool EventModel::removeNextConstraint(id_type<ConstraintModel> constraintToDelete)
{
    return removeConstraint(m_nextConstraints, constraintToDelete);
}

bool EventModel::removePreviousConstraint(id_type<ConstraintModel> constraintToDelete)
{
    return removeConstraint(m_previousConstraints, constraintToDelete);
}

void EventModel::changeTimeNode(id_type<TimeNodeModel> newTimeNodeId)
{
    m_timeNode = newTimeNodeId;
}

id_type<TimeNodeModel> EventModel::timeNode() const
{
    return m_timeNode;
}

double EventModel::heightPercentage() const
{
    return m_heightPercentage;
}

TimeValue EventModel::date() const
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
ScenarioModel* EventModel::parentScenario() const
{
    return dynamic_cast<ScenarioModel*>(parent());
}

QString EventModel::condition() const
{
    return m_condition;
}

const StateList& EventModel::states() const
{
    return m_states;
}

void EventModel::replaceStates(StateList newStates)
{
    m_states = newStates;
}

void EventModel::addState(const State& state)
{
    m_states.append(state);
    emit messagesChanged();
}

void EventModel::removeState(const State& s)
{
    m_states.removeOne(s);
    emit messagesChanged();
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

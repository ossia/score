#include "DisplayedStateModel.hpp"

#include "StateView.hpp"

#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"
#include "Document/Event/EventModel.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

StateModel::StateModel(const id_type<StateModel>& id,
                                         const id_type<EventModel>& eventId,
                                         double yPos,
                                         QObject *parent):
    IdentifiedObject<StateModel> {id, "StateModel", parent},
    m_eventId{eventId},
    m_heightPercentage{yPos}
{

}

StateModel::StateModel(const StateModel &source,
                                         const id_type<StateModel> &id,
                                         QObject *parent):
    StateModel{id, source.eventId(), source.heightPercentage(), parent}
{
    m_states = source.states();
}

const ScenarioInterface* StateModel::parentScenario() const
{
    return dynamic_cast<ScenarioInterface*>(parent());
}

double StateModel::heightPercentage() const
{
    return m_heightPercentage;
}

void StateModel::setHeightPercentage(double y)
{
    if(m_heightPercentage == y)
        return;
    m_heightPercentage = y;
    emit heightPercentageChanged();
}

const id_type<EventModel> &StateModel::eventId() const
{
    return m_eventId;
}

const id_type<ConstraintModel> &StateModel::previousConstraint() const
{
    return m_previousConstraint;
}

const id_type<ConstraintModel> &StateModel::nextConstraint() const
{
    return m_nextConstraint;
}

void StateModel::setNextConstraint(const id_type<ConstraintModel> & id)
{
    m_nextConstraint = id;
}

void StateModel::setPreviousConstraint(const id_type<ConstraintModel> & id)
{
    m_previousConstraint = id;
}


const iscore::StateList& StateModel::states() const
{
    return m_states;
}

void StateModel::replaceStates(const iscore::StateList& newStates)
{
    m_states = newStates;
    emit statesReplaced();
}

void StateModel::addState(const iscore::State &s)
{
    m_states.append(s);
    emit stateAdded(s);
}

void StateModel::removeState(const iscore::State &s)
{
    m_states.removeOne(s);
    emit stateRemoved(s);
}


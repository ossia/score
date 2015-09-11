#include "CSPScenario.hpp"
#include <QDebug>
#include "CSPTimeNode.hpp"
#include "CSPTimeRelation.hpp"
#include <Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Process/ScenarioModel.hpp>

CSPScenario::CSPScenario(const ScenarioModel& scenario)
{
    // Link with i-score
    con(scenario.constraints, &NotifyingMap<ConstraintModel>::added,
        this, &CSPScenario::on_constraintCreated);
    con(scenario.constraints, &NotifyingMap<ConstraintModel>::removed,
        this, &CSPScenario::on_constraintRemoved);

    con(scenario.states, &NotifyingMap<StateModel>::added,
        this, &CSPScenario::on_stateCreated);
    con(scenario.states, &NotifyingMap<StateModel>::removed,
        this, &CSPScenario::on_stateRemoved);

    con(scenario.events, &NotifyingMap<EventModel>::added,
        this, &CSPScenario::on_eventCreated);
    con(scenario.events, &NotifyingMap<EventModel>::removed,
        this, &CSPScenario::on_eventRemoved);

    con(scenario.timeNodes, &NotifyingMap<TimeNodeModel>::added,
        this, &CSPScenario::on_timeNodeCreated);
    con(scenario.timeNodes, &NotifyingMap<TimeNodeModel>::removed,
        this, &CSPScenario::on_timeNodeRemoved);

}

CSPScenario::CSPScenario(const BaseScenario& baseScenario)
{
}

rhea::simplex_solver
CSPScenario::getSolver()
{
    return m_solver;
}

void
CSPScenario::computeAllConstraints()
{

}

void
CSPScenario::on_constraintCreated(const ConstraintModel& constraint)
{
    //create the corresponding time relation
}

void
CSPScenario::on_constraintRemoved(const ConstraintModel& constraint)
{}


void
CSPScenario::on_stateCreated(const StateModel& state)
{}

void
CSPScenario::on_stateRemoved(const StateModel& state)
{}


void
CSPScenario::on_eventCreated(const EventModel& event)
{}

void
CSPScenario::on_eventRemoved(const EventModel& event)
{}


void
CSPScenario::on_timeNodeCreated(const TimeNodeModel& timeNode)
{}

void
CSPScenario::on_timeNodeRemoved(const TimeNodeModel& timeNode)
{}

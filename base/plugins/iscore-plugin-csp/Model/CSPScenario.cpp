#include <QDebug>
#include "CSPScenario.hpp"
#include "CSPTimeNode.hpp"
#include "CSPTimeRelation.hpp"
#include <Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Process/ScenarioModel.hpp>
#include <Process/ScenarioInterface.hpp>

CSPScenario::CSPScenario(const ScenarioModel& scenario)
    :m_scenario(&scenario)
{
    for(auto& timeNodeModel : scenario.timeNodes)
    {
        insertTimenode(timeNodeModel);
    }

    for(auto& constraintModel : scenario.constraints)
    {
        // TODO: make insertconstraint function?
        m_TimeRelations.insert(constraintModel.id(), CSPTimeRelation{*this, constraintModel});
    }

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
    :m_scenario(&baseScenario)
{
}

rhea::simplex_solver
CSPScenario::getSolver()
{
    return m_solver;
}

CSPTimeNode *CSPScenario::getStartTimeNode() const
{
    return m_startTimeNode;
}

CSPTimeNode *CSPScenario::getEndTimeNode() const
{
    return m_endTimeNode;
}

const ScenarioInterface *CSPScenario::getScenario() const
{
    return m_scenario;
}

void CSPScenario::insertTimenode(TimeNodeModel &timeNodeModel)
{
    auto timeNodeId = timeNodeModel.id();

    // if timenode not already here, put ot in
    if(! m_Timenodes.contains(timeNodeId))
    {
        m_Timenodes.insert(timeNodeId, CSPTimeNode{*this, timeNodeModel});
    }
}

void
CSPScenario::computeAllConstraints()
{

}

void
CSPScenario::on_constraintCreated(const ConstraintModel& constraint)
{
    //create the corresponding time relation
    qDebug("coucou?");
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

const
CSPTimeNode&
CSPScenario::getTimenode(ScenarioInterface& scenario, TimeNodeModel& timeNodeModel)
{
    insertTimenode(timeNodeModel);

    return m_Timenodes[timeNodeModel.id()];
}

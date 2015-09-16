#include "CSPScenario.hpp"
#include "CSPTimeNode.hpp"
#include "CSPTimeRelation.hpp"
#include <Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Process/ScenarioModel.hpp>
#include <Process/ScenarioInterface.hpp>
#include <kiwi/kiwi.h>

CSPScenario::CSPScenario(const ScenarioModel& scenario)
    :m_scenario(&scenario)
{
    // ensure that start then end timenode are stored first of all
    m_startTimeNode = insertTimenode(scenario.startTimeNode().id());
    m_endTimeNode = insertTimenode(scenario.endTimeNode().id());

    // insert existing timenodes
    for(auto& timeNodeModel : scenario.timeNodes)
    {
        on_timeNodeCreated(timeNodeModel);
    }

    // insert existing constraints
    for(auto& constraintModel : scenario.constraints)
    {
        on_constraintCreated(constraintModel);
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
    // ensure that start then end timenode are stored first of all
    m_startTimeNode = insertTimenode(baseScenario.startTimeNode().id());
    m_endTimeNode = insertTimenode(baseScenario.endTimeNode().id());

    // insert existing timenodes
    on_timeNodeCreated(baseScenario.startTimeNode());
    on_timeNodeCreated(baseScenario.endTimeNode());

    // insert existing constraints
    auto& constraintModel = baseScenario.baseConstraint();
    on_constraintCreated(constraintModel);
}

kiwi::Solver&
CSPScenario::getSolver()
{
    return m_solver;
}

CSPTimeNode *CSPScenario::getStartTimeNode() const
{
    return m_startTimeNode;
}

CSPTimeNode* CSPScenario::getEndTimeNode() const
{
    return m_endTimeNode;
}

const ScenarioInterface *CSPScenario::getScenario() const
{
    return m_scenario;
}

CSPTimeNode* CSPScenario::insertTimenode(const Id<TimeNodeModel> &timeNodeId)
{

    // if timenode not already here, put ot in
    if(! m_Timenodes.contains(timeNodeId))
    {
        auto cspTimenode = new CSPTimeNode(*this, timeNodeId);
        m_Timenodes.insert(timeNodeId, cspTimenode);
        return cspTimenode;
    }else
    {
        return m_Timenodes[timeNodeId];
    }
}

void
CSPScenario::computeAllConstraints()
{

}

void
CSPScenario::on_constraintCreated(const ConstraintModel& constraintModel)
{
    //create the corresponding time relation
    // TODO: make insertconstraint function?
    m_TimeRelations.insert(constraintModel.id(), new CSPTimeRelation{*this, constraintModel.id()});
}

void
CSPScenario::on_constraintRemoved(const ConstraintModel& constraint)
{

}


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
CSPScenario::on_timeNodeCreated(const TimeNodeModel& timeNodeModel)
{
    insertTimenode(timeNodeModel.id());
}

void
CSPScenario::on_timeNodeRemoved(const TimeNodeModel& timeNode)
{

}

const
CSPTimeNode&
CSPScenario::getTimenode(ScenarioInterface& scenario, const Id<TimeNodeModel>& timeNodeId)
{
    insertTimenode(timeNodeId);

    return *m_Timenodes[timeNodeId];
}

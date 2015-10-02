#include <Model/CSPScenario.hpp>
#include <Model/CSPTimeNode.hpp>
#include <Model/CSPTimeRelation.hpp>
#include <Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Process/ScenarioModel.hpp>
#include <Process/ScenarioInterface.hpp>
#include <kiwi/kiwi.h>
#include <QtAlgorithms>

CSPScenario::CSPScenario(const ScenarioModel& scenario, QObject *parent)
    :QObject::QObject(parent), m_scenario(&scenario)
{
    this->setObjectName("CSPScenario");

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

CSPScenario::CSPScenario(const BaseScenario& baseScenario, QObject *parent)
    :QObject::QObject(parent), m_scenario(&baseScenario)
{
    this->setObjectName("CSPScenario");

    // ensure that start then end timenode are stored first of all
    m_startTimeNode = insertTimenode(baseScenario.startTimeNode().id());
    m_endTimeNode = insertTimenode(baseScenario.endTimeNode().id());

    // insert existing timenodes
    on_timeNodeCreated(baseScenario.startTimeNode());
    on_timeNodeCreated(baseScenario.endTimeNode());

    // insert existing constraints
    on_constraintCreated(baseScenario.baseConstraint());
}

CSPScenario::~CSPScenario()
{
    qDeleteAll(m_timeNodes);
    qDeleteAll(m_timeRelations);
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
    // if timenode not already here, put it in
    if(! m_timeNodes.contains(timeNodeId))
    {
        auto cspTimenode = new CSPTimeNode(*this, timeNodeId);
        m_timeNodes.insert(timeNodeId, cspTimenode);
        return cspTimenode;
    }else
    {
        return m_timeNodes[timeNodeId];
    }
}

CSPTimeRelation *CSPScenario::getTimeRelation(const Id<ConstraintModel> &ConstraintId)
{
    return m_timeRelations[ConstraintId];
}

void
CSPScenario::computeAllConstraints()
{

}

void
CSPScenario::on_constraintCreated(const ConstraintModel& constraintModel)
{
    //create the corresponding time relation
    m_timeRelations.insert(constraintModel.id(), new CSPTimeRelation{*this, constraintModel.id()});
}

void
CSPScenario::on_constraintRemoved(const ConstraintModel& constraint)
{
    delete(m_timeRelations.take(constraint.id()));
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
    delete(m_timeNodes.take(timeNode.id()));
}

const
CSPTimeNode&
CSPScenario::getInsertTimenode(ScenarioInterface& scenario, const Id<TimeNodeModel>& timeNodeId)
{
    insertTimenode(timeNodeId);

    return *m_timeNodes[timeNodeId];
}

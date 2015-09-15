#pragma once

#include <QVector>

#include <iscore/tools/SettableIdentifier.hpp>

#include <kiwi/kiwi.h>

#include "CSPTimeNode.hpp"
#include "CSPTimeRelation.hpp"

class ConstraintModel;
class TimeNodeModel;
class StateModel;
class ScenarioModel;
class BaseScenario;

class CSPScenario : public QObject
{
    Q_OBJECT
public:
    //using QObject::QObject;

    CSPScenario(const ScenarioModel& scenario);
    CSPScenario(const BaseScenario& baseScenario);


    void on_constraintCreated(const ConstraintModel&);
    void on_stateCreated(const StateModel&);
    void on_eventCreated(const EventModel&);
    void on_timeNodeCreated(const TimeNodeModel&);

    void on_constraintRemoved(const ConstraintModel&);
    void on_stateRemoved(const StateModel&);
    void on_eventRemoved(const EventModel&);
    void on_timeNodeRemoved(const TimeNodeModel&);


    const
    CSPTimeNode&
    getTimenode(ScenarioInterface& scenario, const Id<TimeNodeModel>& timeNodeId);

    kiwi::Solver& getSolver();

    CSPTimeNode* getStartTimeNode() const;

    CSPTimeNode* getEndTimeNode() const;

    const ScenarioInterface* getScenario() const;

    CSPTimeNode* insertTimenode(const Id<TimeNodeModel>& timeNodeId);

private:

    QMap<Id<TimeNodeModel>,CSPTimeNode> m_Timenodes;
    QMap<Id<ConstraintModel>,CSPTimeRelation> m_TimeRelations;

    const ScenarioInterface* m_scenario;

    CSPTimeNode* m_startTimeNode;
    CSPTimeNode* m_endTimeNode;


    void computeAllConstraints();

    kiwi::Solver m_solver{};
};

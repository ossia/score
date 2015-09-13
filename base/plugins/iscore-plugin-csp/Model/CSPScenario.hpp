#pragma once

#include <QVector>

#include <rhea/simplex_solver.hpp>

#include <iscore/tools/SettableIdentifier.hpp>

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
    getTimenode(ScenarioInterface& scenario, TimeNodeModel& timeNodeModel);

    rhea::simplex_solver getSolver();

    CSPTimeNode* getStartTimeNode() const;

    CSPTimeNode* getEndTimeNode() const;

    const ScenarioInterface* getScenario() const;

private:

    QMap<Id<TimeNodeModel>,CSPTimeNode> m_Timenodes;
    QMap<Id<ConstraintModel>,CSPTimeRelation> m_TimeRelations;

    const ScenarioInterface* m_scenario;

    CSPTimeNode* m_startTimeNode;
    CSPTimeNode* m_endTimeNode;

    void insertTimenode(TimeNodeModel& timeNodeModel);

    void computeAllConstraints();

    rhea::simplex_solver m_solver;
};

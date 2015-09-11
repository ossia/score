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
    using QObject::QObject;

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

    QMap<Id<TimeNodeModel>,CSPTimeNode> m_Timenodes;
    QMap<Id<ConstraintModel>,CSPTimeRelation> m_TimeRelations;

    rhea::simplex_solver getSolver();

private:

    void computeAllConstraints();

    rhea::simplex_solver m_solver;
};

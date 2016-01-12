#pragma once

#include <QHash>

#include <iscore/tools/SettableIdentifier.hpp>
#include <CSPDisplacementPolicy.hpp>

#include <kiwi/kiwi.h>

class CSPTimeNode;
class CSPTimeRelation;

namespace Scenario
{
class ConstraintModel;
class TimeNodeModel;
class EventModel;
class StateModel;
class ScenarioModel;
class ScenarioInterface;
class BaseScenario;
}

class CSPScenario : public QObject, public Nano::Observer
{
    friend class CSPDisplacementPolicy;
    friend class CSPFlexDisplacementPolicy;

    Q_OBJECT
public:
    //using QObject::QObject;

    CSPScenario(const Scenario::ScenarioModel& scenario, QObject *parent);
    CSPScenario(const Scenario::BaseScenario& baseScenario, QObject *parent);

    ~CSPScenario();

    void on_constraintCreated(const Scenario::ConstraintModel&);
    void on_stateCreated(const Scenario::StateModel&);
    void on_eventCreated(const Scenario::EventModel&);
    void on_timeNodeCreated(const Scenario::TimeNodeModel&);

    void on_constraintRemoved(const Scenario::ConstraintModel&);
    void on_stateRemoved(const Scenario::StateModel&);
    void on_eventRemoved(const Scenario::EventModel&);
    void on_timeNodeRemoved(const Scenario::TimeNodeModel&);


    const CSPTimeNode& getInsertTimenode(
            Scenario::ScenarioInterface& scenario,
            const Id<Scenario::TimeNodeModel>& timeNodeId);

    kiwi::Solver& getSolver();

    CSPTimeNode* getStartTimeNode() const;

    CSPTimeNode* getEndTimeNode() const;

    const Scenario::ScenarioInterface* getScenario() const;

    CSPTimeNode* insertTimenode(
            const Id<Scenario::TimeNodeModel>& timeNodeId);

    CSPTimeRelation* getTimeRelation(
            const Id<Scenario::ConstraintModel>& ConstraintId);

    QHash<Id<Scenario::TimeNodeModel>,CSPTimeNode*> m_timeNodes;
    QHash<Id<Scenario::ConstraintModel>,CSPTimeRelation*> m_timeRelations;

private:

    const Scenario::ScenarioInterface* m_scenario;

    CSPTimeNode* m_startTimeNode;
    CSPTimeNode* m_endTimeNode;


    void computeAllConstraints();

    kiwi::Solver m_solver{};
};

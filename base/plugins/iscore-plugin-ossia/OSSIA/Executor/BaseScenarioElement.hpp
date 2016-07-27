#pragma once
#include <memory>

#include <QObject>
#include <OSSIA/Executor/ConstraintExecutionFacade.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <iscore_plugin_ossia_export.h>
namespace OSSIA
{
    class TimeValue;
}

namespace Scenario
{
class ConstraintModel;
class EventModel;
class StateModel;
class TimeNodeModel;
class BaseScenario;
class ScenarioInterface;
}
class DeviceList;
namespace RecreateOnPlay {
class ConstraintElement;
class EventElement;
class StateElement;
class TimeNodeElement;
}  // namespace RecreateOnPlay


// MOVEME
// Like BaseScenarioContainer but with references
// to existing elements instead.
class BaseScenarioRefContainer
{
    public:
        BaseScenarioRefContainer(
                Scenario::ConstraintModel& constraint,
                Scenario::ScenarioInterface& s);

        BaseScenarioRefContainer(
                Scenario::ConstraintModel& constraint,
                Scenario::StateModel& startState,
                Scenario::StateModel& endState,
                Scenario::EventModel& startEvent,
                Scenario::EventModel& endEvent,
                Scenario::TimeNodeModel& startNode,
                Scenario::TimeNodeModel& endNode):
            m_constraint{constraint},
            m_startState{startState},
            m_endState{endState},
            m_startEvent{startEvent},
            m_endEvent{endEvent},
            m_startNode{startNode},
            m_endNode{endNode}
        {

        }

        Scenario::ConstraintModel& constraint() const { return m_constraint; }

        Scenario::TimeNodeModel& startTimeNode() const { return m_startNode; }
        Scenario::TimeNodeModel& endTimeNode() const { return m_endNode; }

        Scenario::EventModel& startEvent() const { return m_startEvent; }
        Scenario::EventModel& endEvent() const { return m_endEvent; }

        Scenario::StateModel& startState() const { return m_startState; }
        Scenario::StateModel& endState() const { return m_endState; }

    private:
        Scenario::ConstraintModel& m_constraint;
        Scenario::StateModel& m_startState;
        Scenario::StateModel& m_endState;
        Scenario::EventModel& m_startEvent;
        Scenario::EventModel& m_endEvent;
        Scenario::TimeNodeModel& m_startNode;
        Scenario::TimeNodeModel& m_endNode;
};

namespace RecreateOnPlay
{
struct Context;
class ISCORE_PLUGIN_OSSIA_EXPORT BaseScenarioElement final : public QObject
{
        Q_OBJECT
    public:
        BaseScenarioElement(
                BaseScenarioRefContainer element,
                const Context&,
                QObject* parent);

        ConstraintElement* baseConstraint() const;

        TimeNodeElement* startTimeNode() const;
        TimeNodeElement* endTimeNode() const;

        EventElement* startEvent() const;
        EventElement* endEvent() const;

        StateElement* startState() const;
        StateElement* endState() const;

    signals:
        void finished();

    private:
        const Context& m_ctx;
        ConstraintElement* m_ossia_constraint{};

        TimeNodeElement* m_ossia_startTimeNode{};
        TimeNodeElement* m_ossia_endTimeNode{};

        EventElement* m_ossia_startEvent{};
        EventElement* m_ossia_endEvent{};

        StateElement* m_ossia_startState{};
        StateElement* m_ossia_endState{};
};
}

#pragma once
#include <iscore/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp"
#include "Commands/Scenario/Creations/CreationMetaCommand.hpp"

class ScenarioStateMachine;

class CreateFromEventState : public CreationState
{
    public:
        CreateFromEventState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createEventFromEventOnNothing(const ScenarioStateMachine& stateMachine);
        void createEventFromEventOnTimeNode();

        void createConstraintBetweenEvents();
        void createSingleEventOnTimeNode();

        MultiOngoingCommandDispatcher m_dispatcher;
        ScenarioPoint m_clickedPoint;

};

// impl is in CreateEventFromTimeNodeState.cpp
class CreateFromTimeNodeState : public CreationState
{
    public:
        CreateFromTimeNodeState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createSingleEventOnTimeNode();

        void createEventFromEventOnNothing(const ScenarioStateMachine& stateMachine);
        void createEventFromEventOnTimenode();

        void createConstraintBetweenEvents();


        MultiOngoingCommandDispatcher m_dispatcher;

        ScenarioPoint m_clickedPoint;
        id_type<EventModel> m_createdFirstEvent;
};

class CreateFromStateState : public CreationState
{
    public:
        CreateFromStateState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createSingleEventOnTimeNode();

        void createEventFromStateOnNothing(const ScenarioStateMachine& stateMachine);
        void createEventFromStateOnTimeNode();

        void createConstraintBetweenEvents();


        MultiOngoingCommandDispatcher m_dispatcher;
        ScenarioPoint m_clickedPoint;
        id_type<EventModel> m_createdFirstEvent;
};


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
        void createEventFromEventOnNothing();
        void createEventFromEventOnTimeNode();

        void createConstraintBetweenEvents();

        MultiOngoingCommandDispatcher m_dispatcher;
};

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

        void createEventFromEventOnNothing();
        void createEventFromEventOnTimenode();

        void createConstraintBetweenEvents();


        MultiOngoingCommandDispatcher m_dispatcher;

        ScenarioPoint m_clickedPoint;
        id_type<EventModel> m_createdFirstEvent;
};


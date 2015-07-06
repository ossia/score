#pragma once
#include "ScenarioCreationState.hpp"
#include "Commands/Scenario/Creations/CreateState.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/ScenarioModel.hpp"

class ScenarioCreation_FromState : public ScenarioCreationState
{
    public:
        ScenarioCreation_FromState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        const ScenarioStateMachine& m_scenarioSM;
        void createToNothing();
        void createToTimeNode();
        void createToEvent();
        void createToState();

        template<typename Fun>
        void creationCheck(Fun&& fun)
        {
            const auto& scenar = m_scenarioSM.model();
            if(m_scenarioSM.isShiftPressed())
            {
                // Create new state
                auto cmd = new Scenario::Command::CreateState{m_scenarioPath, scenar.state(clickedState).eventId(), currentPoint.y};
                m_dispatcher.submitCommand(cmd);

                createdStates.append(cmd->createdState());
                fun(createdStates.first());
            }
            else
            {
                const auto& st = scenar.state(clickedState);
                if(!st.nextConstraint()) // TODO & deltaY < deltaX
                {
                    currentPoint.y = st.heightPercentage();
                    fun(clickedState);
                }
                else
                {
                    // create a single state on the same event (deltaY > deltaX)
                }
            }
        }
};

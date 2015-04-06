#pragma once
#include "GenericToolState.hpp"

class MoveConstraintState;
class MoveEventState;
class MoveTimeNodeState;
class MoveToolState : public GenericToolState
{
    public:
        MoveToolState(ScenarioStateMachine& sm);

        void on_scenarioPressed() override;
        void on_scenarioMoved() override;
        void on_scenarioReleased() override;

    private:
        MoveConstraintState* m_moveConstraintState{};
        MoveEventState* m_moveEventState{};
        MoveTimeNodeState* m_moveTimeNodeState{};
};

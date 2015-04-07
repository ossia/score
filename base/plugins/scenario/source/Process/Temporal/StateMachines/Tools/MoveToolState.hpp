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
        MoveConstraintState* m_moveConstraint{};
        MoveEventState* m_moveEvent{};
        MoveTimeNodeState* m_moveTimeNode{};
};

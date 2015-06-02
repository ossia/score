#pragma once
#include "ScenarioToolState.hpp"

class MoveConstraintState;
class MoveEventState;
class MoveTimeNodeState;
class MoveToolState : public ScenarioToolState
{
    public:
        MoveToolState(ScenarioStateMachine& sm);

        void on_pressed() override;
        void on_moved() override;
        void on_released() override;

    private:
        MoveConstraintState* m_moveConstraint{};
        MoveEventState* m_moveEvent{};
        MoveTimeNodeState* m_moveTimeNode{};
        QState* m_waitState{};
};

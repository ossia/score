#pragma once
#include <Scenario/Process/Temporal/StateMachines/Tools/ScenarioToolState.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>

// TODO rename file.
namespace Scenario
{
class SelectionState;

class MoveConstraintState;
class MoveEventState;
class MoveTimeNodeState;

class SelectionAndMoveTool final : public ToolBase
{
    public:
        SelectionAndMoveTool(ToolPalette& sm);

        void on_pressed(QPointF scene, Scenario::Point sp);
        void on_moved(QPointF scene, Scenario::Point sp);
        void on_released(QPointF scene, Scenario::Point sp);

    private:
        SelectionState* m_state{};
        MoveConstraintState* m_moveConstraint{};
        MoveEventState* m_moveEvent{};
        MoveTimeNodeState* m_moveTimeNode{};

        bool m_nothingPressed{true};
};
}

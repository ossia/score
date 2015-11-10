#pragma once
#include <iscore/selection/SelectionDispatcher.hpp>
#include "CurveTool.hpp"
class QState;

namespace Curve
{
class OngoingState;
class SelectionState;
class SelectionAndMoveTool final : public CurveTool
{
    public:
        explicit SelectionAndMoveTool(CurveStateMachine& sm);

    protected:

        void on_pressed();

        void on_moved();

        void on_released();
    private:
        Curve::SelectionState* m_state{};
        Curve::OngoingState* m_moveState{};

        std::chrono::steady_clock::time_point m_prev;

        bool m_nothingPressed = false;
};
}

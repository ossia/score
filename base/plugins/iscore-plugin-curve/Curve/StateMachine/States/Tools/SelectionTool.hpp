#pragma once
#include <iscore/selection/SelectionDispatcher.hpp>
#include "CurveTool.hpp"
class QState;

namespace Curve
{
class OngoingState;
class SelectionState;
class SelectionAndMoveTool final : public Curve::CurveTool
{
    public:
        explicit SelectionAndMoveTool(Curve::ToolPalette& sm);

        void on_pressed(QPointF scene, Curve::Point sp);
        void on_moved(QPointF scene, Curve::Point sp);
        void on_released(QPointF scene, Curve::Point sp);

    private:
        Curve::SelectionState* m_state{};
        Curve::OngoingState* m_moveState{};

        bool m_nothingPressed = false;
};
}

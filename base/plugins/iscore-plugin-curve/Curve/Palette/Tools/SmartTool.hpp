#pragma once
#include <QPoint>

#include <Curve/Palette/CurvePoint.hpp>
#include "CurveTool.hpp"

namespace Curve
{
class ToolPalette;
class OngoingState;
class SelectionState;

class SmartTool final : public Curve::CurveTool
{
    public:
        explicit SmartTool(Curve::ToolPalette& sm);

        void on_pressed(QPointF scene, Curve::Point sp);
        void on_moved(QPointF scene, Curve::Point sp);
        void on_released(QPointF scene, Curve::Point sp);

    private:
        Curve::SelectionState* m_state{};
        Curve::OngoingState* m_moveState{};

        bool m_nothingPressed = false;
};
}

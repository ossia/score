#pragma once
#include <Curve/StateMachine/States/Tools/CurveTool.hpp>
namespace Curve
{
class ToolPalette;
class OngoingState;

class EditionToolForCreate : public CurveTool
{
    public:
        explicit EditionToolForCreate(Curve::ToolPalette& sm);

        void on_pressed(QPointF, Curve::Point);
        void on_moved(QPointF, Curve::Point);
        void on_released(QPointF, Curve::Point);
};

class CreateTool final : public Curve::EditionToolForCreate
{
    public:
        explicit CreateTool(Curve::ToolPalette& sm);
};

class SetSegmentTool final : public Curve::EditionToolForCreate
{
    public:
        explicit SetSegmentTool(Curve::ToolPalette& sm);
};

}

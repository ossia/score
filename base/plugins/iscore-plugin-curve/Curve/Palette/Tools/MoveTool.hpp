#pragma once
#include <Curve/Palette/Tools/CurveTool.hpp>
#include <Curve/Palette/CommandObjects/CreatePointCommandObject.hpp>
#include <Curve/Palette/CommandObjects/SetSegmentParametersCommandObject.hpp>
#include <QPoint>

#include <Curve/Palette/CurvePoint.hpp>

namespace Curve
{
class ToolPalette;

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

    private:
        CreatePointCommandObject m_co;
};

class SetSegmentTool final : public Curve::EditionToolForCreate
{
    public:
        explicit SetSegmentTool(Curve::ToolPalette& sm);

    private:
        SetSegmentParametersCommandObject m_co;
};

}

#pragma once
#include <Curve/Palette/Tools/CurveTool.hpp>
#include <qpoint.h>

#include "Curve/Palette/CurvePoint.hpp"

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
};

class SetSegmentTool final : public Curve::EditionToolForCreate
{
    public:
        explicit SetSegmentTool(Curve::ToolPalette& sm);
};

}

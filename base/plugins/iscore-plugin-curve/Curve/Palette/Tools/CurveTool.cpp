#include "CurveTool.hpp"
#include "Curve/Palette/CurvePalette.hpp"

#include "Curve/CurveModel.hpp"
#include "Curve/Palette/OngoingState.hpp"

namespace Curve
{
CurveTool::CurveTool(const Curve::ToolPalette& csm):
    GraphicsSceneTool<Curve::Point>{csm.scene()},
    m_parentSM{csm}
{
}

}


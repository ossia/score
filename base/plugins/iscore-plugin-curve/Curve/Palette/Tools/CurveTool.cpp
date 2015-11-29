#include "Curve/Palette/CurvePalette.hpp"
#include "CurveTool.hpp"
#include "iscore/statemachine/GraphicsSceneTool.hpp"

namespace Curve
{
CurveTool::CurveTool(const Curve::ToolPalette& csm):
    GraphicsSceneTool<Curve::Point>{csm.scene()},
    m_parentSM{csm}
{
}

}


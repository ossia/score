#include "CurveTool.hpp"
#include "Curve/StateMachine/CurveStateMachine.hpp"

#include "Curve/CurveModel.hpp"
#include "Curve/StateMachine/OngoingState.hpp"

namespace Curve
{
CurveTool::CurveTool(const Curve::ToolPalette& csm):
    GraphicsSceneToolBase<Curve::Point>{csm.scene()},
    m_parentSM{csm}
{
}

}


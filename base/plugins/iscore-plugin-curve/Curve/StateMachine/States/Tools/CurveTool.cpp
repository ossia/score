#include "CurveTool.hpp"
#include "Curve/StateMachine/CurveStateMachine.hpp"

#include "Curve/CurveModel.hpp"
#include "Curve/StateMachine/OngoingState.hpp"
#include "Curve/StateMachine/CommandObjects/MovePointCommandObject.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QSignalTransition>
#include "Curve/StateMachine/CommandObjects/CreatePointCommandObject.hpp"
#include "Curve/StateMachine/CommandObjects/SetSegmentParametersCommandObject.hpp"

namespace Curve
{
CurveTool::CurveTool(const Curve::ToolPalette& csm):
    GraphicsSceneToolBase<Curve::Point>{csm.scene()},
    m_parentSM{csm}
{
}

}


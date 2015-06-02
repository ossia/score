#include "CurveTool.hpp"
#include "CurveTest/StateMachine/CurveStateMachine.hpp"


CurveTool::CurveTool(const CurveStateMachine& csm):
    ToolState{csm.scene()},
    m_parentSM{csm}
{

}

#pragma once
#include "CurveTest/StateMachine/States/Tools/CurveTool.hpp"

namespace Curve
{
class MovePointState;
class MoveSegmentState;
class MoveTool : public CurveTool
{
    public:
        MoveTool(CurveStateMachine& sm);

    private:
        QState* m_movePoint{};
        QState* m_moveSegment{};
        QState* m_waitState{};

};

}

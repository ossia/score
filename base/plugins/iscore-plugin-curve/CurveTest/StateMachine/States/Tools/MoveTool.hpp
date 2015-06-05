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

    protected:
        void on_pressed();
        void on_moved();
        void on_released();

    private:
        QState* m_waitState{};

};

}

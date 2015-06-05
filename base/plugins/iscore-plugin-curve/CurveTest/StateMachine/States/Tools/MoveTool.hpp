#pragma once
#include "CurveTest/StateMachine/States/Tools/CurveTool.hpp"

namespace Curve
{
class MovePointState;
class MoveSegmentState;
class EditionTool : public CurveTool
{
    public:
        EditionTool(CurveStateMachine& sm);

    protected:
        void on_pressed();
        void on_moved();
        void on_released();

    private:
        QState* m_waitState{};

};

}

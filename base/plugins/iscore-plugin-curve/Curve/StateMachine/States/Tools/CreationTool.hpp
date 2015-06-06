#pragma once
#include "Curve/StateMachine/States/Tools/CurveTool.hpp"

namespace Curve
{
class CreationTool : public CurveTool
{
    public:
        CreationTool(CurveStateMachine& sm);

    protected:
        void on_pressed();
        void on_moved();
        void on_released();

    private:
        QState* m_createPointFromNothing{};
        QState* m_createPointFromPoint{};
        QState* m_waitState{};

};

}

#pragma once
#include <QState>
class CurveStateMachine;
namespace Curve
{
class MovePointState : public QState
{
    public:
        MovePointState(CurveStateMachine& sm, QState* parent):
            QState{parent}
        {

        }
};
}

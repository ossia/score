#pragma once



#include <qpoint.h>
#include <qstatemachine.h>

class QState;

namespace Scenario
{
class ToolPalette;

class MoveSlotTool
{
    public:
        MoveSlotTool(const Scenario::ToolPalette &sm);

        void on_pressed(QPointF scene);
        void on_moved();
        void on_released();

        void on_cancel();

        void activate();
        void desactivate();

    private:
        QState* m_waitState{};

        QPointF m_originalPoint;
        QStateMachine m_localSM;
        const Scenario::ToolPalette& m_sm;
};
}

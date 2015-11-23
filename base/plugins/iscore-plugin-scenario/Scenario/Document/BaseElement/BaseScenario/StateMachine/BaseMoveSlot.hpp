#pragma once
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <QState>
#include <QStateMachine>

class QGraphicsScene;
class BaseScenarioToolPalette;
class SlotModel;
class BaseMoveSlot
{
    public:
        BaseMoveSlot(
                iscore::CommandStack& stack,
                BaseScenarioToolPalette& sm);

        void on_pressed(QPointF scene);
        void on_moved();
        void on_released();

        void on_cancel();


    private:
        const BaseScenarioToolPalette& m_sm;
        QStateMachine m_localSM;
        QState* m_waitState{};
};


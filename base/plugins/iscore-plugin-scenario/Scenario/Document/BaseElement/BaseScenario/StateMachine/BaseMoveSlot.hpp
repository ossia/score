#pragma once
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <QState>
#include <QStateMachine>

class QGraphicsScene;
class BaseScenarioStateMachine;
class SlotModel;
class BaseMoveSlot
{
    public:
        BaseMoveSlot(
                iscore::CommandStack& stack,
                BaseScenarioStateMachine& sm);

        void on_pressed(QPointF scene);
        void on_moved();
        void on_released();

        void on_cancel();


    private:
        const BaseScenarioStateMachine& m_sm;
        QStateMachine m_localSM;
        QState* m_waitState{};
};


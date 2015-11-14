#pragma once
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <QState>
#include <QStateMachine>
#include <QPointF>

class ScenarioStateMachine;

class SlotModel;
class MoveSlotToolState
{
    public:
        MoveSlotToolState(const ScenarioStateMachine &sm);

        void on_pressed();
        void on_moved();
        void on_released();

        // TODO refactor this with ToolState
        void start();
        void stop();

    private:
        QState* m_waitState{};

        QPointF m_originalPoint;
        QStateMachine m_localSM;
        const ScenarioStateMachine& m_sm;
};

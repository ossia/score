#pragma once
#include "Process/Temporal/StateMachines/Tools/GenericToolState.hpp"
#include <iscore/command/OngoingCommandManager.hpp>


class DeckModel;
class BaseMoveDeck : public QState
{
    public:
        BaseMoveDeck(
                const QGraphicsScene& scene,
                iscore::CommandStack& stack,
                BaseStateMachine& sm);

        void start()
        { m_localSM.start(); }

    private:
        const BaseStateMachine& m_sm;
        QStateMachine m_localSM;
        QState* m_waitState{};

        CommandDispatcher<> m_dispatcher;
        SingleOngoingCommandDispatcher m_ongoingDispatcher;
        const QGraphicsScene& m_scene;
};


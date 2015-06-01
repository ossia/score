#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include <QState>
#include <QStateMachine>

class QGraphicsScene;
class BaseStateMachine;
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


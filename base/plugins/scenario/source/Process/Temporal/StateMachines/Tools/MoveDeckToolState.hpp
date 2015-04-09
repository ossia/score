#pragma once
#include "Process/Temporal/StateMachines/Tools/GenericToolState.hpp"
#include <iscore/command/OngoingCommandManager.hpp>

class GenericToolStateBase : public QState
{
    protected:
        auto itemUnderMouse(const QPointF& point) const
        {
            return m_sm.presenter().view().scene()->itemAt(point, QTransform());
        }

    public:
        GenericToolStateBase(const ScenarioStateMachine& sm) :
            m_sm{sm}
        {
        }


    protected:
        virtual void on_scenarioPressed() = 0;
        virtual void on_scenarioMoved() = 0;
        virtual void on_scenarioReleased() = 0;

        const ScenarioStateMachine& m_sm;
};

class DeckModel;
class MoveDeckToolState : public GenericToolStateBase
{
    public:
        MoveDeckToolState(const ScenarioStateMachine &sm);

        void on_scenarioPressed() override;
        void on_scenarioMoved() override;
        void on_scenarioReleased() override;

    private:
        QState* m_waitState{};
        ObjectPath m_sourceDeck;

        CommandDispatcher<> m_dispatcher;
};

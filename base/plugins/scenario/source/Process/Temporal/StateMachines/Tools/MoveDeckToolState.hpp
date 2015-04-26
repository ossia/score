#pragma once
#include "Process/Temporal/StateMachines/Tools/GenericToolState.hpp"
#include <iscore/command/OngoingCommandManager.hpp>

class GenericStateBase : public QState
{
    public:
        GenericStateBase(const ScenarioStateMachine& sm) :
            m_sm{sm}
        {
        }


    protected:
        QStateMachine m_localSM;
        const ScenarioStateMachine& m_sm;
};

class DeckModel;
class MoveDeckToolState : public GenericStateBase
{
    public:
        MoveDeckToolState(const ScenarioStateMachine &sm);

        void on_scenarioPressed();
        void on_scenarioMoved();
        void on_scenarioReleased();

        void start()
        { m_localSM.start(); }

    private:
        QState* m_waitState{};

        CommandDispatcher<> m_dispatcher;
        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo> m_ongoingDispatcher;

        QPointF m_originalPoint;
        double m_originalHeight{};
};

#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp"
#include "Process/Temporal/StateMachines/Tools/GenericToolState.hpp"
#include <iscore/command/OngoingCommandManager.hpp>

class ResizeDeckState : public DeckState
{
    public:
        ResizeDeckState(
                SingleOngoingCommandDispatcher& dispatcher,
                const BaseStateMachine& sm,
                QState* parent);

    private:
        SingleOngoingCommandDispatcher& m_ongoingDispatcher;
        const BaseStateMachine& m_sm;
};


class DragDeckState : public DeckState
{
    public:
        DragDeckState(
                CommandDispatcher<>& dispatcher,
                const BaseStateMachine& sm,
                const QGraphicsScene& scene,
                QState* parent);

    private:
        CommandDispatcher<> m_dispatcher;
        const BaseStateMachine& m_sm;
        const QGraphicsScene& m_scene;
};

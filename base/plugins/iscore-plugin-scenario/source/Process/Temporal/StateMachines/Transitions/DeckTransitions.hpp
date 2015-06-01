#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

class ClickOnDeckOverlay_Transition : public MatchedTransition<ClickOnDeckOverlay_Event>
{
    public:
        ClickOnDeckOverlay_Transition(DeckState& state);

        DeckState& state() const;

    protected:
        virtual void onTransition(QEvent * ev) override;

    private:
        DeckState& m_state;
};

class ClickOnDeckHandle_Transition : public MatchedTransition<ClickOnDeckHandle_Event>
{
    public:
        ClickOnDeckHandle_Transition(DeckState& state);

        DeckState& state() const;

    protected:
        virtual void onTransition(QEvent * ev) override;

    private:
        DeckState& m_state;
};
